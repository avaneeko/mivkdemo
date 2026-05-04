#ifndef _LOAD_SHADER_MODULE_H_
#define _LOAD_SHADER_MODULE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "vulkan/vulkan.h"

VkResult load_shader_module(VkDevice dev, char const* path, VkShaderModule* out);

#ifdef __cplusplus
}
#endif

#endif
