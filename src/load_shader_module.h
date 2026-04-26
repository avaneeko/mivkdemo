#ifndef _LOAD_SHADER_MODULE_H_
#define _LOAD_SHADER_MODULE_H_

#include "vulkan/vulkan.h"

VkResult load_shader_module(VkDevice dev, char const* path, VkShaderModule* out);

#endif
