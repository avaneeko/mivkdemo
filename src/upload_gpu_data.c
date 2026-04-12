#include "upload_gpu_data.h"
#include "vk.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Creates a staging(CPU-visible) VkBuffer.
static
VkResult create_staging_vkbuf(VkDevice device, VkDeviceSize buffer_size, VkBuffer* out_buf)
{
    VkBufferCreateInfo const info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = buffer_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
    };
    VkResult res = vkCreateBuffer(device, &info, NULL, out_buf);
    return res;
}

// Creates a device-local buffer that's suitable as vertex buffer,
// but is invisible to the host (CPU-side).
static
VkResult create_device_local_vkbuf(VkDevice device, VkDeviceSize buffer_size, VkBuffer* out_buf)
{
    VkBufferCreateInfo const info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .size = buffer_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
    };
    VkResult res = vkCreateBuffer(device, &info, NULL, out_buf);
    return res;
}

static
VkResult alloc_staging_buffer_memory(VkDevice device, VkPhysicalDevice physical_device, VkBuffer buffer, VkDeviceSize allocation_size, VkDeviceMemory* out_memory)
{
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(device, buffer, &mem_req);

    VkMemoryAllocateInfo const info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = mem_req.size,
        .memoryTypeIndex = find_memory_type(physical_device, mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
    };
    VkResult res = vkAllocateMemory(device, &info, NULL, out_memory);
    return res;
}

static
VkResult alloc_device_local_buffer_memory(VkDevice device, VkPhysicalDevice physical_device, VkBuffer buffer, VkDeviceSize allocation_size, VkDeviceMemory* out_memory)
{
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(device, buffer, &mem_req);

    VkMemoryAllocateInfo const info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = NULL,
        .allocationSize = mem_req.size,
        .memoryTypeIndex = find_memory_type(physical_device, mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    VkResult res = vkAllocateMemory(device, &info, NULL, out_memory);
    return res;
}

static
VkResult create_temporary_fence(VkDevice device, VkFence* out_fence)
{
    VkFenceCreateInfo const info =
    {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0  // UNDONE: Verify that these flags are correct.
    };
    VkResult res = vkCreateFence(device, &info, NULL, out_fence);
    return res;
}

static
VkResult allocate_temporary_cmdbuf(VkDevice device, VkCommandPool pool, VkCommandBuffer* out_cmd)
{
    VkCommandBufferAllocateInfo const cmdbufinfo =
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = NULL,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkResult res = vkAllocateCommandBuffers(device, &cmdbufinfo, out_cmd);
    return res;
}

// We will create a temporary upload fence and command buffer and delete it, but in a more production ready API,
// it would be better to just create it once and reuse it.
VkResult instant_upload(VkDevice device, VkCommandPool cmd_pool, VkQueue queue, VkBuffer host_buf, VkBuffer device_buf, VkDeviceSize buf_size)
{
    VkFence fence; // Host->Device-local memory upload fence.
    VkResult res = create_temporary_fence(device, &fence);
    if (res != VK_SUCCESS)
    {
        printf("Fatal error:\r\nvkCreateFence failed during instant_upload.\r\n");
        exit(1);
    }

    // Allocate one command buffer for the transfer operation.
    VkCommandBuffer cmd;
    res = allocate_temporary_cmdbuf(device, cmd_pool, &cmd);
    if (res != VK_SUCCESS)
    {
        printf("Fatal error:\r\nvkAllocateCommandBuffers failed during instant_upload.\r\n");
        exit(1);
    }

    // Begin recording.
    VkCommandBufferBeginInfo const begininfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = 0,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL,
    };
    res = vkBeginCommandBuffer(cmd, &begininfo);
    if (res != VK_SUCCESS)
    {
        printf("Fatal error:\r\nvkBeginCommandBuffer failed during instant_upload.\r\n");
        exit(1);
    }

    VkBufferCopy copyrect =
    {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = buf_size,
    };
    vkCmdCopyBuffer(cmd, host_buf, device_buf, 1, &copyrect);

    // End of recording.
    res = vkEndCommandBuffer(cmd);
    if (res != VK_SUCCESS)
    {
        printf("Fatal error:\r\nvkEndCommandBuffer failed during instant_upload.\r\n");
        exit(1);
    }

    // Submit command to the queue.
    VkSubmitInfo submitinfo =
    {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = NULL,
        .pWaitDstStageMask = 0,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = 0,
    };
    res = vkQueueSubmit(queue, 1, &submitinfo, fence);
    if (res != VK_SUCCESS)
    {
        printf("Fatal error:\r\nvkQueueSubmit failed during instant_upload.\r\n");
        exit(1);
    }
    vkWaitForFences(device, 1, &fence, VK_FALSE, UINT64_MAX);
    if (res != VK_SUCCESS)
    {
        printf("Fatal error:\r\nvkWaitForFences failed during instant_upload.\r\n");
        exit(1);
    }

    vkFreeCommandBuffers(device, cmd_pool, 1, &cmd);
    vkDestroyFence(device, fence, NULL);
    return res;
}

VkResult upload_gpu_data(VkDevice device, VkPhysicalDevice physical, VkCommandPool cmd_pool, VkQueue queue, VkBuffer* out_vertex_buffer, VkDeviceMemory* out_vertex_buffer_memory)
{
    // Test data to upload ;)
    // float4 pos       : 0
    // float3 color     : 16
    // float  padding   : 28
    //
    // Total is 32.
    float const data[] =
    {
         0.0f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 67.0f,
         0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 67.0f,
        -0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 67.0f,
    };

    // Size of the buffer to be created.
    VkDeviceSize buffer_size = sizeof(data);

    // Create the buffer object first.
    VkBuffer buffer; // Staging buffer.
    VkResult res = create_staging_vkbuf(device, buffer_size, &buffer);
    if (res != VK_SUCCESS)
    {
        printf("Fatal error:\r\nvkCreateBuffer failed during upload_gpu_data.\r\n");
        exit(1);
    }

    // Allocate the actual memory for the buffer object.
    VkDeviceMemory memory;
    res = alloc_staging_buffer_memory(device, physical, buffer, buffer_size, &memory);
    if (res != VK_SUCCESS)
    {
        printf("Fatal error:\r\nvkAllocateMemory failed during upload_gpu_data.\r\n");
        exit(1);
    }

    // Bind
    res = vkBindBufferMemory(device, buffer, memory, 0);
    if (res != VK_SUCCESS)
    {
        printf("Fatal error:\r\nvkBindBufferMemory failed during upload_gpu_data.\r\n");
        exit(1);
    }

    // Map, Copy, Unmap.
    VkMemoryMapFlags flags;
    void *mapped_ptr; // Pointer to the mapped memory block.
    res = vkMapMemory(device, memory, 0, buffer_size, 0, &mapped_ptr);
    if (res != VK_SUCCESS)
    {
        printf("Fatal error:\r\nvkMapMemory failed during upload_gpu_data.\r\n");
        exit(1);
    }

    memcpy(mapped_ptr, data, sizeof(data));

    vkUnmapMemory(device, memory);

    // Create device-local buffer.
    VkBuffer device_buffer;
    res = create_device_local_vkbuf(device, buffer_size, &device_buffer);
    if (res != VK_SUCCESS)
    {
        printf("Fatal error:\r\nvkCreateBuffer failed during upload_gpu_data.\r\n");
        exit(1);
    }

    // Allocate actual memory for device-local buffer.
    VkDeviceMemory device_memory;
    res = alloc_device_local_buffer_memory(device, physical, device_buffer, buffer_size, &device_memory);
    if (res != VK_SUCCESS)
    {
        printf("Fatal error:\r\nvkAllocateMemory failed during upload_gpu_data.\r\n");
        exit(1);
    }
    printf("All ok!\r\n");

    // Bind device-local memory to it's buffer object.
    res = vkBindBufferMemory(device, device_buffer, device_memory, 0);
    if (res != VK_SUCCESS)
    {
        printf("Fatal error:\r\nvkBindBufferMemory failed during upload_gpu_data.\r\n");
        exit(1);
    }

    res = instant_upload(device, cmd_pool, queue, buffer, device_buffer, buffer_size);
    assert(res == VK_SUCCESS);

    // Now the resources have been copied to device-local buffer, host-local buffer can be destroyed and it's memory can be freed.
    // Deleting buffer first since it holds a reference to it's VkDeviceMemory.
    vkDestroyBuffer(device, buffer, NULL);
    vkFreeMemory(device, memory, NULL);

    *out_vertex_buffer = device_buffer;
    *out_vertex_buffer_memory = device_memory;

    return res;
}
