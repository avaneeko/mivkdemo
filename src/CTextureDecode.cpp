#include "CTextureDecode.hpp"

#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "third_party/stb_image.h"

namespace
{
    constexpr uint32_t DdsMagic = 0x20534444; // "DDS "
    constexpr size_t DdsHeaderSize = 128;     // Magic + DDS_HEADER.
    constexpr size_t DdsDx10HeaderSize = 20;

    constexpr uint32_t DdsFourCCDx10 = 0x30315844; // "DX10"

    // DXGI_FORMAT values from the DX10 extended header.
    constexpr uint32_t DxgiFormatBc1Unorm = 71;
    constexpr uint32_t DxgiFormatBc1UnormSrgb = 72;

    template <typename T>
    T ReadLE(std::byte const* Base, size_t Offset)
    {
        T Value;
        std::memcpy(&Value, Base + Offset, sizeof(T));
        return Value;
    }

    bool IsDds(std::byte const* Data, size_t Size)
    {
        return Size >= 4 && ReadLE<uint32_t>(Data, 0) == DdsMagic;
    }

    // Only the first mip level is uploaded for now.
    bool DecodeDdsBc1(std::byte const* Data, size_t Size, CDecodedTexture& Out)
    {
        if (Size < DdsHeaderSize)
            return false;

        uint32_t const Height = ReadLE<uint32_t>(Data, 12);
        uint32_t const Width = ReadLE<uint32_t>(Data, 16);
        uint32_t const PixelFormatFourCC = ReadLE<uint32_t>(Data, 84);

        // Legacy DXT1 FourCC is intentionally not handled yet; only DX10 BC1 is accepted.
        if (PixelFormatFourCC != DdsFourCCDx10)
            return false;

        uint32_t const DxgiFormat = ReadLE<uint32_t>(Data, DdsHeaderSize);
        VkFormat Format;
        if (DxgiFormat == DxgiFormatBc1UnormSrgb)
            Format = VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        else if (DxgiFormat == DxgiFormatBc1Unorm)
            Format = VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        else
            return false;

        size_t const DataOffset = DdsHeaderSize + DdsDx10HeaderSize;
        uint32_t const BlocksX = (Width + 3) / 4;
        uint32_t const BlocksY = (Height + 3) / 4;
        size_t const MipSize = static_cast<size_t>(BlocksX) * BlocksY * 8; // 8 bytes per BC1 block.

        if (Size < DataOffset + MipSize)
            return false;

        Out.Width = Width;
        Out.Height = Height;
        Out.Format = Format;
        Out.Pixels.assign(Data + DataOffset, Data + DataOffset + MipSize);
        return true;
    }
}

bool DecodeTextureBytes(std::byte const* Data, size_t Size, bool bSRGB, CDecodedTexture& Out)
{
    if (IsDds(Data, Size))
        return DecodeDdsBc1(Data, Size, Out);

    int Width = 0;
    int Height = 0;
    int Channels = 0;
    unsigned char* Pixels = stbi_load_from_memory(reinterpret_cast<stbi_uc const*>(Data),
        static_cast<int>(Size), &Width, &Height, &Channels, 4);
    if (!Pixels)
        return false;

    Out.Width = static_cast<uint32_t>(Width);
    Out.Height = static_cast<uint32_t>(Height);
    Out.Format = bSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    size_t const ByteCount = static_cast<size_t>(Width) * Height * 4;
    Out.Pixels.assign(reinterpret_cast<std::byte const*>(Pixels),
        reinterpret_cast<std::byte const*>(Pixels) + ByteCount);

    stbi_image_free(Pixels);
    return true;
}

CDecodedTexture MakeCheckerTexture(bool bSRGB)
{
    // Opaque black and white, row-major RGBA8.
    static constexpr uint8_t Texels[2 * 2 * 4] = {
        0,   0,   0,   255,   255, 255, 255, 255,
        255, 255, 255, 255,   0,   0,   0,   255,
    };

    CDecodedTexture Out;
    Out.Width = 2;
    Out.Height = 2;
    Out.Format = bSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    Out.Pixels.resize(sizeof(Texels));
    std::memcpy(Out.Pixels.data(), Texels, sizeof(Texels));
    return Out;
}
