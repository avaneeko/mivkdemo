#ifndef _UPLOAD_GPU_DATA_H_
#define _UPLOAD_GPU_DATA_H_

#include "vulkan/vulkan.h"

VkResult upload_gpu_data(VkDevice device, VkPhysicalDevice physical, VkCommandPool cmd_pool, VkQueue queue, VkBuffer* out_vertex_buffer, VkDeviceMemory* out_vertex_buffer_memory);

#endif
