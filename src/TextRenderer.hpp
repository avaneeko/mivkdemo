#ifndef _TEXT_RENDERER_HPP_
#define _TEXT_RENDERER_HPP_

#include <vulkan/vulkan.h>
#include <vector>
#include <cstddef>

class CTextRenderer
{
public:
    struct BitmapAtlas
    {
        std::byte* Data;
        size_t DataLen;
        bool Load(char const* path);
        void Unload();
    };

    BitmapAtlas Atlas;

    CTextRenderer() = default;
    ~CTextRenderer() = default;

    bool Init();
    bool Shutdown();

    void DrawText();
};

#endif