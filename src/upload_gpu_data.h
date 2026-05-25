#ifndef _UPLOAD_GPU_DATA_H_
#define _UPLOAD_GPU_DATA_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "vulkan/vulkan.h"

// Creates a staging (CPU-visible) buffer for host-to-device transfers.
VkResult create_staging_vkbuf(VkDevice device, VkDeviceSize buffer_size, VkBuffer* out_buf);

// Creates a device-local buffer with TRANSFER_DST_BIT | usage.
VkResult create_device_local_vkbuf_usage_bit(VkDevice device, VkDeviceSize buffer_size, VkBufferUsageFlagBits usage, VkBuffer* out_buf);

// Allocates host-visible + host-coherent memory for a staging buffer.
VkResult alloc_staging_buffer_memory(VkDevice device, VkPhysicalDevice physical_device, VkBuffer buffer, VkDeviceSize allocation_size, VkDeviceMemory* out_memory);

// Allocates device-local memory for a device buffer.
VkResult alloc_device_local_buffer_memory(VkDevice device, VkPhysicalDevice physical_device, VkBuffer buffer, VkDeviceSize allocation_size, VkDeviceMemory* out_memory);

// Submits a one-shot buffer copy (staging → device) and waits for completion via fence.
VkResult instant_upload(VkDevice device, VkCommandPool cmd_pool, VkQueue queue, VkBuffer host_buf, VkBuffer device_buf, VkDeviceSize buf_size);

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

#ifdef __cplusplus
}
#endif

#endif
