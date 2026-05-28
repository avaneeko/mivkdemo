#ifndef _SCENE_LOADER_HPP_
#define _SCENE_LOADER_HPP_

#include "CScene.hpp"
#include "FMesh.hpp"
#include <fastgltf/core.hpp>
#include <vector>

#include <vulkan/vulkan.h>

struct CStagingMesh
{
    std::vector<std::byte> VertexData;
    uint32_t VertexCount;
    uint32_t VertexStride;
    std::vector<uint32_t> IndexData;
    uint32_t IndexCount;
    VkIndexType IndexType;
    uint16_t VertexLayoutId;
    uint16_t MaterialId; // Index into Staging Material Array.
};

struct CStagingMaterial
{
    // UNDONE:
    // Store texture references or something.
};

class SceneLoader
{
    std::vector<CStagingMesh> StagingMeshes;
    std::vector<CStagingMaterial> StagingMaterials;

    VkBuffer MeshBuffer;              // Buffer backing all meshes of the scene.
    VkDeviceMemory MeshMemory;        // It's memory.
    VkDeviceSize MeshMemorySize;      // Total memory size.
    VkDeviceSize MeshMemoryOffset;    // Offset into memory. Bumped when consuming/allocating.
    
    VkBuffer IndexBuffer;             // Buffer backing all mesh index data.
    VkDeviceMemory IndexMemory;       // It's memory.

    std::vector<FMesh> Meshes;        // GPU-ready mesh descriptors, filled by UploadMeshData.
    
    VkBuffer TextureBuffer;           // Buffer backing all textures of the scene.
    VkDeviceMemory TextureMemory;     // It's memory.
    VkDeviceSize TextureMemorySize;   // Total memory size.
    VkDeviceSize TextureMemoryOffset; // Offset into memory. Bumped when consuming/allocating.

    VkDevice Device; // Borrowed device handle.

    // Estimates VkBuffer size for the entire all the scene's meshes.
    VkDeviceSize EstimateSceneMeshMemory(fastgltf::Asset const& gltf);
    // Estimates VkBuffer size for the entire all the scene's textures.
    VkDeviceSize EstimateSceneTextureMemory(fastgltf::Asset const& gltf);
    // Estimates VkBuffer size and requirements for the entire scene.
    VkDeviceSize CalculateBufferSizeRequirements(fastgltf::Asset const& gltf);

    void AllocateMeshBufferAndMemory();
    void AllocateTextureMemory();
    void AllocateVkResources();

public:
    void LoadScene(std::filesystem::path InPath, std::vector<CScene>& OutScenes);

    VkResult UploadMeshData(VkDevice device, VkPhysicalDevice physical, VkCommandPool cmd_pool, VkQueue queue);

    std::vector<FMesh> const& GetMeshes() const { return Meshes; }
};

#endif