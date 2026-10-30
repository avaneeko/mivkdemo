#ifndef _CVERTEX_LAYOUT_LIST_HPP_
#define _CVERTEX_LAYOUT_LIST_HPP_

#include <vulkan/vulkan.h>
#include <array>

class CVertexLayoutList
{
    static auto const Cap = 128; // Hardcap

    std::array<VkVertexInputAttributeDescription*, Cap> List;
};

#endif