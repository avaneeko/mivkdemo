#include "Core.hpp"
#include "TextRenderer.hpp"
#include <cstdio>
#include <fstream>
#include <filesystem>

/// Bitmap Atlas

bool CTextRenderer::BitmapAtlas::Load( char const* path )
{
    std::error_code FileSizeError;
    uintmax_t FileSize = std::filesystem::file_size(path, FileSizeError);

    if ( FileSizeError )
    {
        fprintf(stderr, "CTextRenderer::BitmapAtlas::Load( %s ) : Failed to get file size.\r\n", path);
    }

    std::ifstream File(path);
    if ( not File.is_open() )
    {
        return false;
    }

    Data = reinterpret_cast<std::byte*>(malloc(FileSize));
    DataLen = FileSize;
    check(Data);

    return true;
}

void CTextRenderer::BitmapAtlas::Unload()
{
    free(Data);
    Data = nullptr;
}