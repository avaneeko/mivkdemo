#ifndef _UPLOAD_GPU_DATA_H_
#define _UPLOAD_GPU_DATA_H_

#include "vulkan/vulkan.h"

// UNDONE:
// Creates a vertex buffer.
VkResult create_vertex_buffer(
    VkDevice device,
    VkPhysicalDevice physical,
    VkCommandPool cmd_pool,     // Command pool for allocating the copy command.
    VkQueue queue,              // Queue for submitting the copy command.
    void const* data,                 // Vertex buffer data.
    size_t data_size,           // Size of vertex buffer data in bytes.
    VkBuffer* out_buffer,       // Ptr to place newly created buffer.
    VkDeviceMemory* out_vertex_buffer_memory // Ptr to place newly created device memory associated with the buffer.
);

// Creates an index buffer.
VkResult create_index_buffer(
    VkDevice device,
    VkPhysicalDevice physical,
    VkCommandPool cmd_pool,     // Command pool for allocating the copy command.
    VkQueue queue,              // Queue for submitting the copy command.
    void const* data,                 // index buffer data.
    size_t data_size,           // Size of index buffer data in bytes.
    VkBuffer* out_buffer,       // Ptr to place newly created buffer.
    VkDeviceMemory* out_index_buffer_memory // Ptr to place newly created device memory associated with the buffer.
);

VkResult upload_gpu_data(VkDevice device, VkPhysicalDevice physical, VkCommandPool cmd_pool, VkQueue queue, VkBuffer* out_vertex_buffer, VkDeviceMemory* out_vertex_buffer_memory);

#endif
