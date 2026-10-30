#include "ScenePipeline.hpp"
#include "Core.hpp"
#include "load_shader_module.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

#define SCENE_VS_PATH "c:/stuff/mivkdemo/shaders/scene.vs.spv"
#define SCENE_PS_PATH "c:/stuff/mivkdemo/shaders/scene.ps.spv"

VkResult CreateScenePipeline(VkDevice device, VkFormat colorFormat, VkFormat depthFormat,
                             FScenePipeline* OutPipeline)
{
    *OutPipeline = {};

    // Load shaders
    VkShaderModule vs, ps;
    {
        VkResult vsRes = load_shader_module(device, SCENE_VS_PATH, &vs);
        VkResult psRes = load_shader_module(device, SCENE_PS_PATH, &ps);
        if (vsRes != VK_SUCCESS || psRes != VK_SUCCESS) {
            fprintf(stderr, "Failed to load scene shader modules.\r\n");
            return vsRes != VK_SUCCESS ? vsRes : psRes;
        }
    }

    VkPipelineShaderStageCreateInfo stages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vs,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = ps,
            .pName = "main",
        },
    };

    // Vertex input — stride 32, 3 attributes
    VkVertexInputBindingDescription binding = {
        .binding = 0,
        .stride = 32,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkVertexInputAttributeDescription attributes[] = {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = 0,
        },
        {
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = 12,
        },
        {
            .location = 2,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = 24,
        },
    };

    VkPipelineVertexInputStateCreateInfo vertexInput = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding,
        .vertexAttributeDescriptionCount = ARRAYSIZE(attributes),
        .pVertexAttributeDescriptions = attributes,
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo rasterization = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineDepthStencilStateCreateInfo depthStencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
    };

    VkPipelineColorBlendAttachmentState blendAttachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo colorBlend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blendAttachment,
    };

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = ARRAYSIZE(dynamicStates),
        .pDynamicStates = dynamicStates,
    };

    // Push constants: { modelView, proj, baseColorTexture } = 132 bytes
    VkPushConstantRange pushRange = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(float) * 32 + sizeof(uint32_t),
    };

    VkDescriptorSetLayoutBinding bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = SCENE_MAX_TEXTURES,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };

    VkDescriptorBindingFlags bindingFlags[] = {
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
        0,
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = ARRAYSIZE(bindingFlags),
        .pBindingFlags = bindingFlags,
    };

    VkDescriptorSetLayoutCreateInfo setLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &bindingFlagsInfo,
        .bindingCount = ARRAYSIZE(bindings),
        .pBindings = bindings,
    };

    VkResult res = vkCreateDescriptorSetLayout(device, &setLayoutInfo, nullptr, &OutPipeline->TextureSetLayout);
    if (res != VK_SUCCESS) {
        vkDestroyShaderModule(device, vs, nullptr);
        vkDestroyShaderModule(device, ps, nullptr);
        return res;
    }

    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &OutPipeline->TextureSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushRange,
    };

    res = vkCreatePipelineLayout(device, &layoutInfo, nullptr, &OutPipeline->Layout);
    if (res != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(device, OutPipeline->TextureSetLayout, nullptr);
        vkDestroyShaderModule(device, vs, nullptr);
        vkDestroyShaderModule(device, ps, nullptr);
        return res;
    }

    VkDescriptorPoolSize poolSizes[] = {
        { .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = SCENE_MAX_TEXTURES },
        { .type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 1 },
    };

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = ARRAYSIZE(poolSizes),
        .pPoolSizes = poolSizes,
    };

    res = vkCreateDescriptorPool(device, &poolInfo, nullptr, &OutPipeline->TexturePool);
    if (res != VK_SUCCESS) {
        vkDestroyPipelineLayout(device, OutPipeline->Layout, nullptr);
        vkDestroyDescriptorSetLayout(device, OutPipeline->TextureSetLayout, nullptr);
        vkDestroyShaderModule(device, vs, nullptr);
        vkDestroyShaderModule(device, ps, nullptr);
        return res;
    }

    VkDescriptorSetAllocateInfo setAlloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = OutPipeline->TexturePool,
        .descriptorSetCount = 1,
        .pSetLayouts = &OutPipeline->TextureSetLayout,
    };

    res = vkAllocateDescriptorSets(device, &setAlloc, &OutPipeline->TextureSet);
    if (res != VK_SUCCESS) {
        vkDestroyDescriptorPool(device, OutPipeline->TexturePool, nullptr);
        vkDestroyPipelineLayout(device, OutPipeline->Layout, nullptr);
        vkDestroyDescriptorSetLayout(device, OutPipeline->TextureSetLayout, nullptr);
        vkDestroyShaderModule(device, vs, nullptr);
        vkDestroyShaderModule(device, ps, nullptr);
        return res;
    }

    // Dynamic rendering — no render pass needed
    VkPipelineRenderingCreateInfo renderingInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colorFormat,
        .depthAttachmentFormat = depthFormat,
    };

    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderingInfo,
        .stageCount = ARRAYSIZE(stages),
        .pStages = stages,
        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterization,
        .pMultisampleState = &multisample,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlend,
        .pDynamicState = &dynamicState,
        .layout = OutPipeline->Layout,
    };

    res = vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineInfo, nullptr, &OutPipeline->Pipeline);

    vkDestroyShaderModule(device, vs, nullptr);
    vkDestroyShaderModule(device, ps, nullptr);

    return res;
}

VkResult WriteSceneTextures(VkDevice device, FScenePipeline const& Pipeline, gpu_texture_block const& Textures, VkSampler Sampler)
{
    if (Textures.count > SCENE_MAX_TEXTURES) {
        fprintf(stderr, "Scene has %u textures, more than the %d descriptor slots.\r\n", Textures.count, SCENE_MAX_TEXTURES);
        return VK_ERROR_TOO_MANY_OBJECTS;
    }

    std::vector<VkDescriptorImageInfo> ImageInfos(Textures.count);
    for (uint32_t i = 0; i < Textures.count; ++i)
        ImageInfos[i] = { VK_NULL_HANDLE, Textures.views[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

    VkDescriptorImageInfo SamplerInfo = { Sampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED };

    VkWriteDescriptorSet Writes[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = Pipeline.TextureSet,
            .dstBinding = 0,
            .descriptorCount = Textures.count,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .pImageInfo = ImageInfos.data(),
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = Pipeline.TextureSet,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .pImageInfo = &SamplerInfo,
        },
    };

    vkUpdateDescriptorSets(device, ARRAYSIZE(Writes), Writes, 0, nullptr);
    return VK_SUCCESS;
}

void DestroyScenePipeline(VkDevice device, FScenePipeline Pipeline)
{
    if (Pipeline.Pipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(device, Pipeline.Pipeline, nullptr);
    if (Pipeline.Layout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(device, Pipeline.Layout, nullptr);
    if (Pipeline.TexturePool != VK_NULL_HANDLE)
        vkDestroyDescriptorPool(device, Pipeline.TexturePool, nullptr);
    if (Pipeline.TextureSetLayout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(device, Pipeline.TextureSetLayout, nullptr);
}
