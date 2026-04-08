#ifndef _TRI_PIPELINE_H_
#define _TRI_PIPELINE_H_

#include <vulkan/vulkan.h>

VkResult create_tri_pipeline(VkDevice device, VkFormat color_format, VkFormat depth_format, VkPipeline* pipeline, VkPipelineLayout* layout);

#endif
