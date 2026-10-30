#ifndef _SCENE_PIPELINE_HPP_
#define _SCENE_PIPELINE_HPP_

#include "texture_gpu.h"

#include <vulkan/vulkan.h>

// Capacity of the scene texture array. Slots past the uploaded textures stay unbound.
#define SCENE_MAX_TEXTURES 256

struct FScenePipeline {
    VkPipeline Pipeline;
    VkPipelineLayout Layout;
    VkDescriptorSetLayout TextureSetLayout;
    VkDescriptorPool TexturePool;
    VkDescriptorSet TextureSet;
};

// Creates a graphics pipeline for rendering scene meshes.
// Vertex format: stride 32, pos(f3)@0, nrm(f3)@12, tex(f2)@24
// Push constants: { modelView: f4x4, proj: f4x4, baseColorTexture: u32 } (132 bytes total)
VkResult CreateScenePipeline(VkDevice device, VkFormat colorFormat, VkFormat depthFormat,
                             FScenePipeline* OutPipeline);

// Writes the scene texture array and sampler into the pipeline's descriptor set.
VkResult WriteSceneTextures(VkDevice device, FScenePipeline const& Pipeline, gpu_texture_block const& Textures, VkSampler Sampler);

void DestroyScenePipeline(VkDevice device, FScenePipeline Pipeline);

#endif
