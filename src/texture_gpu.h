#ifndef _TEXTURE_GPU_H_
#define _TEXTURE_GPU_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "vulkan/vulkan.h"

// pixels holds mip_levels tightly packed levels, largest first. Uncompressed sources with fewer
// levels than the full chain get the rest generated on upload.
typedef struct {
    void const* pixels;
    uint32_t width;
    uint32_t height;
    VkFormat format;
    uint32_t mip_levels;
} gpu_texture_source;

// Every image in a block shares one VkDeviceMemory; each image is bound at its own offset.
typedef struct {
    VkDeviceMemory memory;
    VkImage* images;
    VkImageView* views;
    uint32_t count;
} gpu_texture_block;

// Creates, binds and uploads all textures into one device-local memory block.
// Images are left in SHADER_READ_ONLY_OPTIMAL. On failure, out is left fully destroyed.
VkResult create_texture_block(
    VkDevice device,
    VkPhysicalDevice physical,
    VkCommandPool cmd_pool,
    VkQueue queue,
    gpu_texture_source const* sources,
    uint32_t count,
    gpu_texture_block* out);

void destroy_texture_block(VkDevice device, gpu_texture_block* block);

VkResult create_texture_sampler(VkDevice device, VkSampler* out_sampler);

#ifdef __cplusplus
}
#endif

#endif
