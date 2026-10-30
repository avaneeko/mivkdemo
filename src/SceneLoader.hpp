#ifndef _SCENE_LOADER_HPP_
#define _SCENE_LOADER_HPP_

#include "CScene.hpp"
#include "FMesh.hpp"
#include "CTextureDecode.hpp"
#include "texture_gpu.h"
#include "third_party/cgltf.h"
#include <filesystem>
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

// Each slot is an index into SceneLoader::StagingTextures. Missing slots reference a checker texture.
struct CStagingMaterial
{
    uint32_t BaseColorTexture;
    uint32_t MetallicRoughnessTexture;
    uint32_t NormalTexture;
    uint32_t OcclusionTexture;
    uint32_t EmissiveTexture;
};

class SceneLoader
{
    std::vector<CStagingMesh> StagingMeshes;
    std::vector<CStagingMaterial> StagingMaterials;
    std::vector<CDecodedTexture> StagingTextures; // Decoded on CPU; uploaded by UploadTextureData.

    VkBuffer MeshBuffer;              // Buffer backing all meshes of the scene.
    VkDeviceMemory MeshMemory;        // It's memory.
    
    VkBuffer IndexBuffer;             // Buffer backing all mesh index data.
    VkDeviceMemory IndexMemory;       // It's memory.

    std::vector<FMesh> Meshes;        // GPU-ready mesh descriptors, filled by UploadMeshData.

    gpu_texture_block Textures;       // All scene textures, in one device memory block.
    VkSampler TextureSampler;

    // Estimates VkBuffer size for the entire all the scene's meshes.
    VkDeviceSize EstimateSceneMeshMemory(cgltf_data const* gltf);

public:
    void LoadScene(std::filesystem::path InPath, std::vector<CScene>& OutScenes);

    VkResult UploadMeshData(VkDevice device, VkPhysicalDevice physical, VkCommandPool cmd_pool, VkQueue queue);
    VkResult UploadTextureData(VkDevice device, VkPhysicalDevice physical, VkCommandPool cmd_pool, VkQueue queue);

    std::vector<FMesh> const& GetMeshes() const { return Meshes; }
    gpu_texture_block const& GetTextures() const { return Textures; }
    VkSampler GetTextureSampler() const { return TextureSampler; }
};

#endif
