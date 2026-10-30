#ifndef _FTEXTURE_DECODE_HPP_
#define _FTEXTURE_DECODE_HPP_

#include <cstddef>
#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

// CPU-side texture data ready for upload. Pixels are tightly packed for Format.
struct CDecodedTexture
{
    std::vector<std::byte> Pixels;
    uint32_t Width = 0;
    uint32_t Height = 0;
    VkFormat Format = VK_FORMAT_UNDEFINED;
};

// Decodes an encoded image (PNG/JPEG/etc via stb_image, or DDS containing BC1).
// sRGB selects the color-space format for stb-decoded data (base color, emissive);
// data textures (normal, metal/rough, occlusion) pass false.
bool DecodeTextureBytes(std::byte const* Data, size_t Size, bool bSRGB, CDecodedTexture& Out);

// Returns a 2x2 black and white checkerboard in RGBA8.
CDecodedTexture MakeCheckerTexture(bool bSRGB);

#endif
