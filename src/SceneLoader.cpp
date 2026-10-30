#include "SceneLoader.hpp"
#include "Core.hpp"
#include "FMat4.hpp"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <string>

#include "upload_gpu_data.h"

static
VkDeviceSize AlignUp(VkDeviceSize InAddr, VkDeviceSize InAlignment)
{
    return (InAddr + InAlignment - 1) / InAlignment * InAlignment;
}

VkDeviceSize SceneLoader::EstimateSceneMeshMemory(cgltf_data const* gltf)
{
    VkDeviceSize Total = 0;
    VkDeviceSize const BufferAlignment = 64; // Pessimistic guess. UNDONE: Probe with a dummy buffer?

    for (cgltf_size m = 0; m < gltf->meshes_count; ++m) {
        cgltf_mesh const& Mesh = gltf->meshes[m];
        for (cgltf_size p = 0; p < Mesh.primitives_count; ++p) {
            cgltf_primitive const& Primitive = Mesh.primitives[p];

            if (Primitive.indices) {
                Total += AlignUp(Primitive.indices->count * Primitive.indices->stride, BufferAlignment);
            }

            for (cgltf_size a = 0; a < Primitive.attributes_count; ++a) {
                cgltf_accessor const* Accessor = Primitive.attributes[a].data;
                Total += AlignUp(Accessor->count * Accessor->stride, BufferAlignment);
            }
        }
    }

    return Total;
}

namespace
{
    cgltf_accessor const* FindAttribute(cgltf_primitive const& Primitive, cgltf_attribute_type Type)
    {
        for (cgltf_size i = 0; i < Primitive.attributes_count; ++i) {
            cgltf_attribute const& Attribute = Primitive.attributes[i];
            if (Attribute.type == Type && Attribute.index == 0)
                return Attribute.data;
        }
        return nullptr;
    }

    // Primitives without POSITION are skipped everywhere, so staging and instance indices stay in step.
    bool IsDrawablePrimitive(cgltf_primitive const& Primitive)
    {
        return FindAttribute(Primitive, cgltf_attribute_type_position) != nullptr;
    }

    // Writes Elements floats per vertex at Dst, with a 32-byte vertex stride.
    void CopyVertexAttribute(cgltf_accessor const* Accessor, uint32_t VertexCount, cgltf_size Elements, std::byte* Dst)
    {
        for (uint32_t i = 0; i < VertexCount; ++i) {
            float Value[4] = {};
            cgltf_accessor_read_float(Accessor, i, Value, Elements);
            std::memcpy(Dst + (size_t)i * 32, Value, Elements * sizeof(float));
        }
    }
}

void ExtractStagingMeshesFromGtlf(cgltf_data const* gltf, std::vector<CStagingMesh>& OutStagingMeshes)
{
    for (cgltf_size m = 0; m < gltf->meshes_count; ++m) {
        cgltf_mesh const& gltfMesh = gltf->meshes[m];
        for (cgltf_size p = 0; p < gltfMesh.primitives_count; ++p) {
            cgltf_primitive const& primitive = gltfMesh.primitives[p];
            if (!IsDrawablePrimitive(primitive)) {
                fprintf(stderr, "WARNING: Skipping primitive %zu of mesh %zu: no POSITION attribute.\r\n", p, m);
                continue;
            }

            CStagingMesh Mesh;
            cgltf_accessor const* PosAcc = FindAttribute(primitive, cgltf_attribute_type_position);
            cgltf_accessor const* NrmAcc = FindAttribute(primitive, cgltf_attribute_type_normal);
            cgltf_accessor const* TexAcc = FindAttribute(primitive, cgltf_attribute_type_texcoord);

            Mesh.VertexCount = (uint32_t)PosAcc->count;
            Mesh.VertexStride = 32; // 3+3+2 floats
            Mesh.VertexData.resize(Mesh.VertexCount * Mesh.VertexStride); // Zero-filled, so missing NORMAL/UVs read as 0.

            CopyVertexAttribute(PosAcc, Mesh.VertexCount, 3, Mesh.VertexData.data());
            if (NrmAcc)
                CopyVertexAttribute(NrmAcc, Mesh.VertexCount, 3, Mesh.VertexData.data() + 12);
            if (TexAcc)
                CopyVertexAttribute(TexAcc, Mesh.VertexCount, 2, Mesh.VertexData.data() + 24);

            if (primitive.indices) {
                cgltf_accessor const* IdxAcc = primitive.indices;
                bool const bU16 = IdxAcc->component_type == cgltf_component_type_r_16u;
                Mesh.IndexCount = (uint32_t)IdxAcc->count;
                Mesh.IndexType = bU16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;

                size_t const ElementSize = bU16 ? sizeof(uint16_t) : sizeof(uint32_t);
                Mesh.IndexData.resize((Mesh.IndexCount * ElementSize + sizeof(uint32_t) - 1) / sizeof(uint32_t));
                std::byte* Dst = reinterpret_cast<std::byte*>(Mesh.IndexData.data());
                for (uint32_t i = 0; i < Mesh.IndexCount; ++i) {
                    uint32_t const Index = (uint32_t)cgltf_accessor_read_index(IdxAcc, i);
                    if (bU16) {
                        uint16_t const Index16 = (uint16_t)Index;
                        std::memcpy(Dst + i * sizeof(uint16_t), &Index16, sizeof(Index16));
                    } else {
                        std::memcpy(Dst + i * sizeof(uint32_t), &Index, sizeof(Index));
                    }
                }
            }

            // The default material is appended after the glTF materials.
            Mesh.MaterialId = primitive.material
                ? (uint16_t)cgltf_material_index(gltf, primitive.material)
                : (uint16_t)gltf->materials_count;

            OutStagingMeshes.push_back(std::move(Mesh));
        }
    }
}

namespace
{
    // Fixed slots at the front of StagingTextures; every missing material slot points at one of these.
    constexpr uint32_t CheckerSRGBTexture = 0;
    constexpr uint32_t CheckerLinearTexture = 1;

    int Base64Value(char c)
    {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    }

    bool DecodeBase64(std::string const& Text, std::vector<std::byte>& Out)
    {
        Out.clear();
        uint32_t Accumulator = 0;
        int Bits = 0;
        for (char c : Text) {
            if (c == '=')
                break;
            if (c == '\r' || c == '\n' || c == ' ')
                continue;
            int const Value = Base64Value(c);
            if (Value < 0)
                return false;
            Accumulator = (Accumulator << 6) | (uint32_t)Value;
            Bits += 6;
            if (Bits >= 8) {
                Bits -= 8;
                Out.push_back(std::byte((Accumulator >> Bits) & 0xFF));
            }
        }
        return true;
    }

    bool ReadFileBytes(std::filesystem::path const& Path, std::vector<std::byte>& Out)
    {
        std::ifstream File(Path, std::ios::binary | std::ios::ate);
        if (!File)
            return false;
        std::streamoff const Size = File.tellg();
        Out.resize((size_t)Size);
        File.seekg(0);
        return static_cast<bool>(File.read(reinterpret_cast<char*>(Out.data()), Size));
    }

    // Returns the image bytes. Buffer-view bytes point into the cgltf_data; data URIs and
    // external files are read into Storage, which the caller keeps alive while decoding.
    bool GetImageBytes(cgltf_image const* Image, std::filesystem::path const& BaseDir,
        std::vector<std::byte>& Storage, std::byte const*& Data, size_t& Size)
    {
        if (Image->buffer_view) {
            Data = reinterpret_cast<std::byte const*>(cgltf_buffer_view_data(Image->buffer_view));
            Size = Image->buffer_view->size;
            return Data != nullptr;
        }

        if (!Image->uri)
            return false;

        std::string const Uri = Image->uri;
        constexpr char DataPrefix[] = "data:";
        if (Uri.compare(0, sizeof(DataPrefix) - 1, DataPrefix) == 0) {
            size_t const Comma = Uri.find(',');
            if (Comma == std::string::npos || Uri.substr(0, Comma).find(";base64") == std::string::npos)
                return false;
            if (!DecodeBase64(Uri.substr(Comma + 1), Storage))
                return false;
        } else {
            std::string Decoded = Uri;
            Decoded.resize(cgltf_decode_uri(Decoded.data()));
            if (!ReadFileBytes(BaseDir / Decoded, Storage))
                return false;
        }

        Data = Storage.data();
        Size = Storage.size();
        return true;
    }

    // Maps glTF texture references to StagingTextures indices, decoding each (image, colour space) pair once.
    struct TextureTable
    {
        cgltf_data const* Gltf;
        std::filesystem::path const& BaseDir;
        std::vector<CDecodedTexture>& Textures;
        std::map<std::pair<cgltf_image const*, bool>, uint32_t> ImageCache;

        uint32_t Resolve(cgltf_texture_view const& View, bool bSRGB)
        {
            uint32_t const Checker = bSRGB ? CheckerSRGBTexture : CheckerLinearTexture;
            cgltf_image const* Image = View.texture ? View.texture->image : nullptr;
            if (!Image)
                return Checker;

            auto const Key = std::make_pair(Image, bSRGB);
            if (auto const It = ImageCache.find(Key); It != ImageCache.end())
                return It->second;

            uint32_t Index = Checker;
            std::vector<std::byte> Storage;
            std::byte const* Data = nullptr;
            size_t Size = 0;
            CDecodedTexture Decoded;
            if (GetImageBytes(Image, BaseDir, Storage, Data, Size) && DecodeTextureBytes(Data, Size, bSRGB, Decoded)) {
                Index = (uint32_t)Textures.size();
                Textures.push_back(std::move(Decoded));
            } else {
                fprintf(stderr, "WARNING: Could not decode image %zu, using checker texture.\r\n",
                    (size_t)(Image - Gltf->images));
            }

            ImageCache.emplace(Key, Index);
            return Index;
        }
    };
}

void ExtractStagingMaterialsFromGltf(cgltf_data const* gltf, std::vector<CStagingMaterial>& OutStagingMaterials,
    std::vector<CDecodedTexture>& OutStagingTextures, std::filesystem::path const& BaseDir)
{
    OutStagingTextures.clear();
    OutStagingTextures.push_back(MakeCheckerTexture(true));
    OutStagingTextures.push_back(MakeCheckerTexture(false));

    OutStagingMaterials.clear();
    TextureTable Table{ gltf, BaseDir, OutStagingTextures, {} };

    for (cgltf_size i = 0; i < gltf->materials_count; ++i)
    {
        cgltf_material const& Material = gltf->materials[i];
        CStagingMaterial Staging;
        Staging.BaseColorTexture = Table.Resolve(Material.pbr_metallic_roughness.base_color_texture, true);
        Staging.MetallicRoughnessTexture = Table.Resolve(Material.pbr_metallic_roughness.metallic_roughness_texture, false);
        Staging.NormalTexture = Table.Resolve(Material.normal_texture, false);
        Staging.OcclusionTexture = Table.Resolve(Material.occlusion_texture, false);
        Staging.EmissiveTexture = Table.Resolve(Material.emissive_texture, true);
        OutStagingMaterials.push_back(Staging);
    }

    // Used by primitives that reference no material.
    OutStagingMaterials.push_back(CStagingMaterial{
        CheckerSRGBTexture, CheckerLinearTexture, CheckerLinearTexture, CheckerLinearTexture, CheckerSRGBTexture });
}

namespace
{
    void EmitNodeInstances(cgltf_data const* Gltf, cgltf_node const* Node,
        std::vector<uint32_t> const& MeshPrimitiveStart, CScene& Scene)
    {
        if (Node->mesh) {
            uint32_t const MeshIdx = (uint32_t)cgltf_mesh_index(Gltf, Node->mesh);

            float WorldMatrix[16];
            cgltf_node_transform_world(Node, WorldMatrix);
            // Convert from glTF Y-up RH to our Z-up LH coordinate system
            FMat4 World = FMat4::ConvertYUpRHToZUpLH(FMat4::FromFloat16(WorldMatrix));

            // Emit one instance per primitive (each primitive is a separate staging mesh + FMesh)
            uint32_t StagingIdx = MeshPrimitiveStart[MeshIdx];
            for (cgltf_size p = 0; p < Node->mesh->primitives_count; ++p) {
                if (!IsDrawablePrimitive(Node->mesh->primitives[p]))
                    continue;

                FSceneInstance Instance;
                Instance.MeshIndex = StagingIdx++;
                for (int i = 0; i < 16; i++)
                    Instance.WorldMatrix[i] = World[i];
                Scene.Instances.push_back(Instance);
            }
        }

        for (cgltf_size c = 0; c < Node->children_count; ++c)
            EmitNodeInstances(Gltf, Node->children[c], MeshPrimitiveStart, Scene);
    }
}

void SceneLoader::LoadScene(std::filesystem::path InPath, std::vector<CScene>& OutScenes)
{
    std::string const PathString = InPath.generic_string();

    cgltf_options Options = {};
    cgltf_data* Gltf = nullptr;
    if (cgltf_parse_file(&Options, PathString.c_str(), &Gltf) != cgltf_result_success)
    {
        fprintf(stderr, "Fatal error: Could not open or parse %s scene file.\r\n", PathString.c_str());
        return;
    }
    if (cgltf_load_buffers(&Options, Gltf, PathString.c_str()) != cgltf_result_success)
    {
        fprintf(stderr, "Fatal error: Could not load buffers of %s scene.\r\n", PathString.c_str());
        cgltf_free(Gltf);
        return;
    }

    VkDeviceSize SceneMemorySize = EstimateSceneMeshMemory(Gltf);
    fprintf(stderr, "Estimated scene mesh memory requirement is: %zu\r\n", SceneMemorySize);

    StagingMeshes.clear();
    StagingMeshes.reserve(Gltf->meshes_count);

    ExtractStagingMaterialsFromGltf(Gltf, StagingMaterials, StagingTextures, InPath.parent_path());
    ExtractStagingMeshesFromGtlf(Gltf, StagingMeshes);

    // Build mapping: for each glTF mesh, which staging mesh index is its first primitive
    std::vector<uint32_t> MeshPrimitiveStart;
    MeshPrimitiveStart.reserve(Gltf->meshes_count);
    uint32_t stagingIdx = 0;
    for (cgltf_size m = 0; m < Gltf->meshes_count; ++m) {
        MeshPrimitiveStart.push_back(stagingIdx);
        for (cgltf_size p = 0; p < Gltf->meshes[m].primitives_count; ++p) {
            if (IsDrawablePrimitive(Gltf->meshes[m].primitives[p]))
                ++stagingIdx;
        }
    }

    // Build one CScene per glTF scene
    OutScenes.clear();
    OutScenes.reserve(Gltf->scenes_count);

    for (cgltf_size si = 0; si < Gltf->scenes_count; si++)
    {
        cgltf_scene const& gltfScene = Gltf->scenes[si];
        CScene Scene;
        Scene.Name = gltfScene.name ? gltfScene.name : "";

        for (cgltf_size n = 0; n < gltfScene.nodes_count; ++n)
            EmitNodeInstances(Gltf, gltfScene.nodes[n], MeshPrimitiveStart, Scene);

        OutScenes.push_back(std::move(Scene));
    }

    cgltf_free(Gltf);

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
        TotalIndexSize += sm.IndexCount * (sm.IndexType == VK_INDEX_TYPE_UINT16 ? sizeof(uint16_t) : sizeof(uint32_t));
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
            VkDeviceSize meshIndexSize = sm.IndexCount * (sm.IndexType == VK_INDEX_TYPE_UINT16 ? sizeof(uint16_t) : sizeof(uint32_t));
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

    return VK_SUCCESS;
}

void SceneLoader::DestroyGpuData(VkDevice Device)
{
    if (TextureSampler != VK_NULL_HANDLE)
        vkDestroySampler(Device, TextureSampler, nullptr);
    TextureSampler = VK_NULL_HANDLE;
    destroy_texture_block(Device, &Textures);

    if (IndexBuffer != VK_NULL_HANDLE)
        vkDestroyBuffer(Device, IndexBuffer, nullptr);
    if (IndexMemory != VK_NULL_HANDLE)
        vkFreeMemory(Device, IndexMemory, nullptr);
    IndexBuffer = VK_NULL_HANDLE;
    IndexMemory = VK_NULL_HANDLE;

    if (MeshBuffer != VK_NULL_HANDLE)
        vkDestroyBuffer(Device, MeshBuffer, nullptr);
    if (MeshMemory != VK_NULL_HANDLE)
        vkFreeMemory(Device, MeshMemory, nullptr);
    MeshBuffer = VK_NULL_HANDLE;
    MeshMemory = VK_NULL_HANDLE;
}

VkResult SceneLoader::UploadTextureData(VkDevice Device, VkPhysicalDevice Physical,
    VkCommandPool CmdPool, VkQueue Queue)
{
    std::vector<gpu_texture_source> Sources;
    Sources.reserve(StagingTextures.size());
    for (auto const& Texture : StagingTextures)
        Sources.push_back(gpu_texture_source{ Texture.Pixels.data(), Texture.Width, Texture.Height, Texture.Format, Texture.MipLevels });

    VkResult Result = create_texture_block(Device, Physical, CmdPool, Queue,
        Sources.data(), (uint32_t)Sources.size(), &Textures);
    if (Result != VK_SUCCESS)
        return Result;

    Result = create_texture_sampler(Device, &TextureSampler);
    if (Result != VK_SUCCESS)
    {
        destroy_texture_block(Device, &Textures);
        return Result;
    }

    return VK_SUCCESS;
}
