#ifndef VK_H
#define VK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <windows.h>

typedef struct {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkSurfaceKHR surface;
    VkDevice device;
    VkQueue queue;
    VkSwapchainKHR swapchain;
    VkFormat surface_format;
    VkImage* swapchain_images;
    uint32_t swapchain_image_count;
    VkImageView* swapchain_image_views;
    uint32_t swapchain_image_view_count;
    VkExtent2D window_size;
    uint32_t queue_index;
    VkImage depth_buffer;
    VkImageView depth_buffer_view;
    VkCommandPool command_pools[2];
	VkCommandBuffer command_buffers[2];
    VkFence render_fences[2];
    VkSemaphore swapchain_semaphores[2];
    VkSemaphore render_semaphores[2];
} vulkan;

/* Initialize vulkan. */
VkResult vk_init(vulkan* vk, HWND hwnd, unsigned short width, unsigned short height);
uint32_t find_memory_type(VkPhysicalDevice physical_device, uint32_t memory_type_bits, VkMemoryPropertyFlags required_properties);

#ifdef __cplusplus
}
#endif

#endif
