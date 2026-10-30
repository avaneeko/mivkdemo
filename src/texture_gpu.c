#include "texture_gpu.h"
#include "upload_gpu_data.h"
#include "vk.h"

#include <stdlib.h>
#include <string.h>

// Staging copies must start on a texel-block boundary; 16 satisfies every supported format.
#define STAGING_ALIGNMENT 16

static VkDeviceSize align_up(VkDeviceSize value, VkDeviceSize alignment)
{
    return (value + alignment - 1) / alignment * alignment;
}

// Byte size of one tightly packed mip level. Extend here when adding new formats.
static VkDeviceSize texture_byte_size(VkFormat format, uint32_t width, uint32_t height)
{
    switch (format) {
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_R8G8B8A8_UNORM:
        return (VkDeviceSize)width * height * 4;
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        return (VkDeviceSize)((width + 3) / 4) * ((height + 3) / 4) * 8;
    default:
        return 0;
    }
}

static VkResult create_image_handles(VkDevice device, VkPhysicalDevice physical, gpu_texture_source const* sources, uint32_t count, gpu_texture_block* out, VkDeviceSize* image_offsets, VkDeviceSize* byte_sizes, VkDeviceSize* staging_offsets, VkDeviceSize* out_staging_size)
{
    VkDeviceSize block_size = 0;
    VkDeviceSize staging_size = 0;
    uint32_t memory_type_bits = 0xFFFFFFFF;

    for (uint32_t i = 0; i < count; i++) {
        gpu_texture_source const* src = &sources[i];

        byte_sizes[i] = texture_byte_size(src->format, src->width, src->height);
        if (byte_sizes[i] == 0)
            return VK_ERROR_FORMAT_NOT_SUPPORTED;

        VkImageCreateInfo const info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = src->format,
            .extent = { src->width, src->height, 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = NULL,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        VkResult res = vkCreateImage(device, &info, NULL, &out->images[i]);
        if (res != VK_SUCCESS)
            return res;

        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(device, out->images[i], &req);
        memory_type_bits &= req.memoryTypeBits;

        block_size = align_up(block_size, req.alignment);
        image_offsets[i] = block_size;
        block_size += req.size;

        staging_size = align_up(staging_size, STAGING_ALIGNMENT);
        staging_offsets[i] = staging_size;
        staging_size += byte_sizes[i];
    }

    uint32_t const memory_type = find_memory_type(physical, memory_type_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memory_type == 0xFFFFFFFF)
        return VK_ERROR_FEATURE_NOT_PRESENT;

    VkMemoryAllocateInfo const alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = block_size,
        .memoryTypeIndex = memory_type,
    };
    VkResult res = vkAllocateMemory(device, &alloc, NULL, &out->memory);
    if (res != VK_SUCCESS)
        return res;

    for (uint32_t i = 0; i < count; i++) {
        res = vkBindImageMemory(device, out->images[i], out->memory, image_offsets[i]);
        if (res != VK_SUCCESS)
            return res;
    }

    *out_staging_size = staging_size;
    return VK_SUCCESS;
}

static VkResult upload_texture_pixels(VkDevice device, VkPhysicalDevice physical, VkCommandPool cmd_pool, VkQueue queue, gpu_texture_source const* sources, uint32_t count, gpu_texture_block const* block, VkDeviceSize const* byte_sizes, VkDeviceSize const* staging_offsets, VkDeviceSize staging_size)
{
    VkBuffer staging;
    VkResult res = create_staging_vkbuf(device, staging_size, &staging);
    if (res != VK_SUCCESS)
        return res;

    VkDeviceMemory staging_mem;
    res = alloc_staging_buffer_memory(device, physical, staging, staging_size, &staging_mem);
    if (res != VK_SUCCESS) {
        vkDestroyBuffer(device, staging, NULL);
        return res;
    }

    res = vkBindBufferMemory(device, staging, staging_mem, 0);
    if (res != VK_SUCCESS)
        goto cleanup_staging;

    void* mapped;
    res = vkMapMemory(device, staging_mem, 0, staging_size, 0, &mapped);
    if (res != VK_SUCCESS)
        goto cleanup_staging;

    for (uint32_t i = 0; i < count; i++)
        memcpy((char*)mapped + staging_offsets[i], sources[i].pixels, byte_sizes[i]);
    vkUnmapMemory(device, staging_mem);

    VkCommandBufferAllocateInfo const cmd_alloc = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer cmd;
    res = vkAllocateCommandBuffers(device, &cmd_alloc, &cmd);
    if (res != VK_SUCCESS)
        goto cleanup_staging;

    VkCommandBufferBeginInfo const begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL,
    };
    vkBeginCommandBuffer(cmd, &begin);

    VkImageSubresourceRange const whole_image = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    for (uint32_t i = 0; i < count; i++) {
        VkImageMemoryBarrier to_transfer = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = block->images[i],
            .subresourceRange = whole_image,
        };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &to_transfer);

        VkBufferImageCopy const region = {
            .bufferOffset = staging_offsets[i],
            .bufferRowLength = 0,   // Tightly packed.
            .bufferImageHeight = 0,
            .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
            .imageOffset = { 0, 0, 0 },
            .imageExtent = { sources[i].width, sources[i].height, 1 },
        };
        vkCmdCopyBufferToImage(cmd, staging, block->images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        VkImageMemoryBarrier to_shader = to_transfer;
        to_shader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        to_shader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        to_shader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_shader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &to_shader);
    }

    vkEndCommandBuffer(cmd);

    VkFenceCreateInfo const fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };
    VkFence fence;
    vkCreateFence(device, &fence_info, NULL, &fence);

    VkSubmitInfo const submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
    };
    res = vkQueueSubmit(queue, 1, &submit, fence);
    if (res == VK_SUCCESS)
        res = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

    vkDestroyFence(device, fence, NULL);
    vkFreeCommandBuffers(device, cmd_pool, 1, &cmd);
    vkDestroyBuffer(device, staging, NULL);
    vkFreeMemory(device, staging_mem, NULL);
    return res;

cleanup_staging:
    vkDestroyBuffer(device, staging, NULL);
    vkFreeMemory(device, staging_mem, NULL);
    return res;
}

static VkResult create_image_views(VkDevice device, gpu_texture_source const* sources, uint32_t count, gpu_texture_block* out)
{
    for (uint32_t i = 0; i < count; i++) {
        VkImageViewCreateInfo const info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .image = out->images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = sources[i].format,
            .components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        };
        VkResult res = vkCreateImageView(device, &info, NULL, &out->views[i]);
        if (res != VK_SUCCESS)
            return res;
    }
    return VK_SUCCESS;
}

void destroy_texture_block(VkDevice device, gpu_texture_block* block)
{
    for (uint32_t i = 0; i < block->count; i++) {
        vkDestroyImageView(device, block->views[i], NULL);
        vkDestroyImage(device, block->images[i], NULL);
    }
    vkFreeMemory(device, block->memory, NULL);
    free(block->views);
    free(block->images);
    memset(block, 0, sizeof(*block));
}

VkResult create_texture_block(VkDevice device, VkPhysicalDevice physical, VkCommandPool cmd_pool, VkQueue queue, gpu_texture_source const* sources, uint32_t count, gpu_texture_block* out)
{
    memset(out, 0, sizeof(*out));
    if (count == 0)
        return VK_SUCCESS;

    out->count = count;
    out->images = calloc(count, sizeof(VkImage));
    out->views = calloc(count, sizeof(VkImageView));
    VkDeviceSize* image_offsets = malloc(count * sizeof(VkDeviceSize));
    VkDeviceSize* byte_sizes = malloc(count * sizeof(VkDeviceSize));
    VkDeviceSize* staging_offsets = malloc(count * sizeof(VkDeviceSize));
    VkDeviceSize staging_size = 0;

    VkResult res = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (out->images && out->views && image_offsets && byte_sizes && staging_offsets) {
        res = create_image_handles(device, physical, sources, count, out, image_offsets, byte_sizes, staging_offsets, &staging_size);
        if (res == VK_SUCCESS)
            res = upload_texture_pixels(device, physical, cmd_pool, queue, sources, count, out, byte_sizes, staging_offsets, staging_size);
        if (res == VK_SUCCESS)
            res = create_image_views(device, sources, count, out);
    }

    free(image_offsets);
    free(byte_sizes);
    free(staging_offsets);

    if (res != VK_SUCCESS)
        destroy_texture_block(device, out);
    return res;
}

VkResult create_texture_sampler(VkDevice device, VkSampler* out_sampler)
{
    VkSamplerCreateInfo const info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };
    return vkCreateSampler(device, &info, NULL, out_sampler);
}
