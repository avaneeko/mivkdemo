#include "SceneLoader.hpp"
#include "Core.hpp"
#include <fastgltf/tools.hpp>
#include <cstdio>

#include "upload_gpu_data.h"

using fastgltf::Expected;
using fastgltf::GltfDataBuffer;
using fastgltf::Asset;

void PrintMatrix(fastgltf::math::fmat4x4 const& InMatrix)
{
    float const* Data = InMatrix.data();
    printf("%f %f %f %f\r\n%f %f %f %f\r\n%f %f %f %f\r\n%f %f %f %f\r\n\r\n",
        Data[0],  Data[1],  Data[2],  Data[3],
        Data[4],  Data[5],  Data[6],  Data[7],
        Data[8],  Data[9],  Data[10], Data[11],
        Data[12], Data[13], Data[14], Data[15]);
}

static
VkDeviceSize AlignUp(VkDeviceSize InAddr, VkDeviceSize InAlignment)
{
    return (InAddr + InAlignment - 1) / InAlignment * InAlignment;
}

VkDeviceSize SceneLoader::EstimateSceneMeshMemory(fastgltf::Asset const& gltf)
{
    VkDeviceSize Total = 0;
    VkDeviceSize const BufferAlignment = 64; // Pessimistic guess. UNDONE: Probe with a dummy buffer?

    for (const auto& Mesh : gltf.meshes)
    {
        for (const auto& primitive : Mesh.primitives) {
            // Index buffer
            if (primitive.indicesAccessor.has_value()) {
                const auto& accessor = gltf.accessors[*primitive.indicesAccessor];
                VkDeviceSize size = accessor.count * fastgltf::getElementByteSize(
                    accessor.type, accessor.componentType);
                Total += AlignUp(size, BufferAlignment);
            }
            
            // Vertex attributes (POSITION, NORMAL, TEXCOORD_0, TANGENT
            for (const auto& attribute : primitive.attributes) {
                const auto& accessor = gltf.accessors[attribute.accessorIndex];
                VkDeviceSize size = accessor.count * fastgltf::getElementByteSize(
                    accessor.type, accessor.componentType);
                Total += AlignUp(size, BufferAlignment);
            }
        }
    }

    return Total;
}

VkDeviceSize SceneLoader::EstimateSceneTextureMemory(fastgltf::Asset const& gltf)
{
    VkDeviceSize Total = 0;

    return Total;
}

VkDeviceSize SceneLoader::CalculateBufferSizeRequirements(fastgltf::Asset const& gltf)
{
    return EstimateSceneMeshMemory(gltf) + EstimateSceneTextureMemory(gltf);
}

void SceneLoader::AllocateMeshBufferAndMemory()
{
    // VkBufferCreateInfo const info = {
    //     .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    //     .pNext = NULL,
    //     .flags = 0,
    //     .size = MeshMemorySize,
    //     .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    //     .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    //     .queueFamilyIndexCount = 0,
    //     .pQueueFamilyIndices = NULL,
    // };
    // VkResult res = vkCreateBuffer(device, &info, NULL, &MeshBuffer);
}

void SceneLoader::AllocateTextureMemory()
{
    // Should this allocate images and image views?
    fprintf(stderr, "WARNING: AllocateTextureMemory() function is not implemented! Did"
        "not allocate any memory for scene textures.\r\n");

    TextureBuffer = VK_NULL_HANDLE;
    TextureMemory = VK_NULL_HANDLE;
    TextureMemoryOffset = 0;
}

void SceneLoader::AllocateVkResources()
{
    AllocateMeshBufferAndMemory();
    AllocateTextureMemory();
}

void ExtractStagingMeshesFromGtlf(fastgltf::Asset const& gltf, std::vector<CStagingMesh>& OutStagingMeshes, std::vector<CStagingMaterial>& OutStagingMaterials)
{
    for (auto const& gltfMesh : gltf.meshes) {
        for (auto const& primitive : gltfMesh.primitives) {
            CStagingMesh Mesh;
            
            // Interleave vertex data using fastgltf accessor iteration
            auto const& posAcc = gltf.accessors[primitive.findAttribute("POSITION")->accessorIndex];
            auto const& nrmAcc = gltf.accessors[primitive.findAttribute("NORMAL")->accessorIndex];
            auto const& texAcc = gltf.accessors[primitive.findAttribute("TEXCOORD_0")->accessorIndex];
            
            Mesh.VertexCount = (uint32_t)posAcc.count;
            Mesh.VertexStride = 32; // 3+3+2 floats
            Mesh.VertexData.resize(Mesh.VertexCount * Mesh.VertexStride);
            
            // Use copyFromAccessor with TargetStride to interleave into the vertex buffer
            fastgltf::copyFromAccessor<fastgltf::math::fvec3, 32>(gltf, posAcc, Mesh.VertexData.data());
            fastgltf::copyFromAccessor<fastgltf::math::fvec3, 32>(gltf, nrmAcc, Mesh.VertexData.data() + 12);
            fastgltf::copyFromAccessor<fastgltf::math::fvec2, 32>(gltf, texAcc, Mesh.VertexData.data() + 24);
            
            // Index data
            if (primitive.indicesAccessor.has_value()) {
                auto const& idxAcc = gltf.accessors[*primitive.indicesAccessor];
                Mesh.IndexCount = (uint32_t)idxAcc.count;
                size_t idxSize = fastgltf::getElementByteSize(idxAcc.type, idxAcc.componentType);
                Mesh.IndexData.resize(Mesh.IndexCount * (idxAcc.componentType == fastgltf::ComponentType::UnsignedShort ? 2 : 4));
                
                Mesh.IndexType = (idxAcc.componentType == fastgltf::ComponentType::UnsignedShort)
                    ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
                if (Mesh.IndexType == VK_INDEX_TYPE_UINT16)
                {
                    fastgltf::copyFromAccessor<uint16_t>(gltf, idxAcc, Mesh.IndexData.data());
                }
                else
                {
                    check(Mesh.IndexType == VK_INDEX_TYPE_UINT32);
                    fastgltf::copyFromAccessor<uint32_t>(gltf, idxAcc, Mesh.IndexData.data());
                }
            }
            
            // Material lookup
            if (primitive.materialIndex.has_value()) {
                // Convert glTF material → FStagingMaterial if not already done
                Mesh.MaterialId = (uint16_t)*primitive.materialIndex;
            }

            OutStagingMeshes.push_back(std::move(Mesh));
        }
    }
}

void SceneLoader::LoadScene(std::filesystem::path InPath, std::vector<CScene>& OutScenes)
{
    Expected<GltfDataBuffer> File = fastgltf::GltfDataBuffer::FromPath(InPath);
    if (File.error() != fastgltf::Error::None)
    {
        fprintf(stderr, "Fatal error: Could not open %s scene file.\r\n", InPath.generic_string().c_str());
        return;
    }

    fastgltf::Parser Parser;
    Expected<fastgltf::Asset> Asset = Parser.loadGltf(File.get(), InPath.root_path());
    if (Asset.error() != fastgltf::Error::None)
    {
        fprintf(stderr, "Fatal error: Parsing of %s scene has failed.\r\n", InPath.generic_string().c_str());
        return;
    }

    VkDeviceSize SceneMemorySize = EstimateSceneMeshMemory(Asset.get());
    fprintf(stderr, "Estimated scene mesh memory requirement is: %zu\r\n", SceneMemorySize);

    StagingMeshes.clear();
    StagingMeshes.reserve(Asset->meshes.size());
    StagingMaterials.clear();

    ExtractStagingMeshesFromGtlf(Asset.get(), StagingMeshes, StagingMaterials);

    // Build one CScene per glTF scene
    OutScenes.clear();
    OutScenes.reserve(Asset->scenes.size());

    for (size_t si = 0; si < Asset->scenes.size(); si++)
    {
        auto const& gltfScene = Asset->scenes[si];
        CScene Scene;
        Scene.Name = gltfScene.name;

        fastgltf::iterateSceneNodes(Asset.get(), si, fastgltf::math::fmat4x4(1.0f),
            [&](fastgltf::Node& node, fastgltf::math::fmat4x4 const& worldMatrix) {
                if (node.meshIndex.has_value()) {
                    FSceneInstance Instance;
                    Instance.MeshIndex = (uint32_t)*node.meshIndex;
                    // Copy the 4x4 matrix into our flat float array (column-major)
                    float const* src = worldMatrix.data();
                    for (int i = 0; i < 16; i++)
                        Instance.WorldMatrix[i] = src[i];
                    Scene.Instances.push_back(std::move(Instance));
                }
            });

        OutScenes.push_back(std::move(Scene));
    }

    printf("Loaded %zu scene(s), %zu total mesh instance(s)\r\n",
        OutScenes.size(), StagingMeshes.size());
}

VkResult SceneLoader::UploadMeshData(VkDevice Device, VkPhysicalDevice Physical,
    VkCommandPool CmdPool, VkQueue Queue)
{
    // Calculate total sizes
    VkDeviceSize TotalVertexSize = 0;
    VkDeviceSize TotalIndexSize = 0;
    for (auto const& sm : StagingMeshes) {
        TotalVertexSize += sm.VertexData.size();
        TotalIndexSize += sm.IndexData.size() * sizeof(uint32_t);
    }

    if (TotalVertexSize == 0) {
        fprintf(stderr, "UploadMeshData: No vertex data to upload.\r\n");
        return VK_SUCCESS;
    }

    VkResult Result;

    //
    // Vertex buffer
    //
    {
        VkBuffer StagingBuf;
        Result = create_staging_vkbuf(Device, TotalVertexSize, &StagingBuf);
        if (Result != VK_SUCCESS) return Result;

        VkDeviceMemory StagingMem;
        Result = alloc_staging_buffer_memory(Device, Physical, StagingBuf, TotalVertexSize, &StagingMem);
        if (Result != VK_SUCCESS) { vkDestroyBuffer(Device, StagingBuf, NULL); return Result; }

        Result = vkBindBufferMemory(Device, StagingBuf, StagingMem, 0);
        if (Result != VK_SUCCESS) { vkDestroyBuffer(Device, StagingBuf, NULL); vkFreeMemory(Device, StagingMem, NULL); return Result; }

        void* mapped;
        Result = vkMapMemory(Device, StagingMem, 0, TotalVertexSize, 0, &mapped);
        if (Result != VK_SUCCESS) { vkDestroyBuffer(Device, StagingBuf, NULL); vkFreeMemory(Device, StagingMem, NULL); return Result; }

        // Copy each staging mesh's vertex data into the combined staging buffer
        VkDeviceSize offset = 0;
        for (auto& sm : StagingMeshes) {
            std::memcpy(static_cast<std::byte*>(mapped) + offset, sm.VertexData.data(), sm.VertexData.size());
            offset += sm.VertexData.size();
        }
        vkUnmapMemory(Device, StagingMem);

        // Device-local vertex buffer
        Result = create_device_local_vkbuf_usage_bit(Device, TotalVertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &MeshBuffer);
        if (Result != VK_SUCCESS) { vkDestroyBuffer(Device, StagingBuf, NULL); vkFreeMemory(Device, StagingMem, NULL); return Result; }

        Result = alloc_device_local_buffer_memory(Device, Physical, MeshBuffer, TotalVertexSize, &MeshMemory);
        if (Result != VK_SUCCESS) { vkDestroyBuffer(Device, StagingBuf, NULL); vkDestroyBuffer(Device, MeshBuffer, NULL); vkFreeMemory(Device, StagingMem, NULL); return Result; }

        Result = vkBindBufferMemory(Device, MeshBuffer, MeshMemory, 0);
        if (Result != VK_SUCCESS) { vkDestroyBuffer(Device, StagingBuf, NULL); vkDestroyBuffer(Device, MeshBuffer, NULL); vkFreeMemory(Device, MeshMemory, NULL); vkFreeMemory(Device, StagingMem, NULL); return Result; }

        // Copy staging to device-local
        Result = instant_upload(Device, CmdPool, Queue, StagingBuf, MeshBuffer, TotalVertexSize);
        if (Result != VK_SUCCESS) return Result;

        // Free staging
        vkDestroyBuffer(Device, StagingBuf, NULL);
        vkFreeMemory(Device, StagingMem, NULL);
    }

    //
    // Index buffer
    //
    {
        VkBuffer stagingBuf;
        Result = create_staging_vkbuf(Device, TotalIndexSize, &stagingBuf);
        if (Result != VK_SUCCESS) return Result;

        VkDeviceMemory stagingMem;
        Result = alloc_staging_buffer_memory(Device, Physical, stagingBuf, TotalIndexSize, &stagingMem);
        if (Result != VK_SUCCESS) { vkDestroyBuffer(Device, stagingBuf, NULL); return Result; }

        Result = vkBindBufferMemory(Device, stagingBuf, stagingMem, 0);
        if (Result != VK_SUCCESS) { vkDestroyBuffer(Device, stagingBuf, NULL); vkFreeMemory(Device, stagingMem, NULL); return Result; }

        void* mapped;
        Result = vkMapMemory(Device, stagingMem, 0, TotalIndexSize, 0, &mapped);
        if (Result != VK_SUCCESS) { vkDestroyBuffer(Device, stagingBuf, NULL); vkFreeMemory(Device, stagingMem, NULL); return Result; }

        VkDeviceSize idxOffset = 0;
        for (auto& sm : StagingMeshes) {
            VkDeviceSize meshIndexSize = sm.IndexData.size() * sizeof(uint32_t);
            memcpy(static_cast<std::byte*>(mapped) + idxOffset, sm.IndexData.data(), meshIndexSize);
            idxOffset += meshIndexSize;
        }
        vkUnmapMemory(Device, stagingMem);

        // Device-local index buffer
        Result = create_device_local_vkbuf_usage_bit(Device, TotalIndexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, &IndexBuffer);
        if (Result != VK_SUCCESS) { vkDestroyBuffer(Device, stagingBuf, NULL); vkFreeMemory(Device, stagingMem, NULL); return Result; }

        Result = alloc_device_local_buffer_memory(Device, Physical, IndexBuffer, TotalIndexSize, &IndexMemory);
        if (Result != VK_SUCCESS) { vkDestroyBuffer(Device, stagingBuf, NULL); vkDestroyBuffer(Device, IndexBuffer, NULL); vkFreeMemory(Device, stagingMem, NULL); return Result; }

        Result = vkBindBufferMemory(Device, IndexBuffer, IndexMemory, 0);
        if (Result != VK_SUCCESS) { vkDestroyBuffer(Device, stagingBuf, NULL); vkDestroyBuffer(Device, IndexBuffer, NULL); vkFreeMemory(Device, IndexMemory, NULL); vkFreeMemory(Device, stagingMem, NULL); return Result; }

        Result = instant_upload(Device, CmdPool, Queue, stagingBuf, IndexBuffer, TotalIndexSize);
        if (Result != VK_SUCCESS) return Result;

        vkDestroyBuffer(Device, stagingBuf, NULL);
        vkFreeMemory(Device, stagingMem, NULL);
    }

    //
    // Build FMesh array — one per staging mesh, pointing into the combined buffers
    //
    Meshes.clear();
    Meshes.reserve(StagingMeshes.size());

    VkDeviceSize vbOffset = 0;
    VkDeviceSize ibOffset = 0;

    for (auto const& sm : StagingMeshes) {
        FMesh Mesh;
        Mesh.VertexBuffer = MeshBuffer;
        Mesh.VertexBufferOffset = vbOffset;
        Mesh.VertexBufferSize = sm.VertexData.size();

        Mesh.IndexBuffer = IndexBuffer;
        Mesh.IndexBufferOffset = ibOffset;
        Mesh.IndexBufferSize = sm.IndexCount * (sm.IndexType == VK_INDEX_TYPE_UINT16 ? sizeof(uint16_t) : sizeof(uint32_t));
        Mesh.IndexBufferType = sm.IndexType;

        Mesh.VertexLayoutId = sm.VertexLayoutId;
        Mesh.MaterialId = sm.MaterialId;

        Meshes.push_back(Mesh);

        vbOffset += sm.VertexData.size();
        ibOffset += sm.IndexCount * (sm.IndexType == VK_INDEX_TYPE_UINT16 ? sizeof(uint16_t) : sizeof(uint32_t));
    }

    printf("Uploaded %zu mesh(es): vertex buffer %llu bytes, index buffer %llu bytes\r\n",
        Meshes.size(), (unsigned long long)TotalVertexSize, (unsigned long long)TotalIndexSize);

    MeshMemorySize = TotalVertexSize;
    MeshMemoryOffset = 0;

    return VK_SUCCESS;
}