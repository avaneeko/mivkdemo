#ifndef _MESH_HPP_
#define _MESH_HPP_

#include <vulkan/vulkan.h>

struct FMesh
{
    VkBuffer VertexBuffer;
    VkDeviceSize VertexBufferOffset;
    VkDeviceSize VertexBufferSize;

    VkBuffer IndexBuffer;
    VkDeviceSize IndexBufferOffset;
    VkDeviceSize IndexBufferSize;
    VkIndexType IndexBufferType;

    uint16_t VertexLayoutId; // Index into Vertex Layout Array.
    uint16_t MaterialId;     // Index into Material Array.
};

#endif