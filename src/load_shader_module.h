#ifndef _LOAD_SHADER_MODULE_
#define _LOAD_SHADER_MODULE_

#include "vulkan/vulkan.h"

VkResult load_shader_module(VkDevice dev, char const* path, VkShaderModule* out);

#endif
