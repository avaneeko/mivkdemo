#ifndef _SCENE_PIPELINE_HPP_
#define _SCENE_PIPELINE_HPP_

#include <vulkan/vulkan.h>

struct FScenePipeline {
    VkPipeline Pipeline;
    VkPipelineLayout Layout;
};

// Creates a graphics pipeline for rendering scene meshes.
// Vertex format: stride 32, pos(f3)@0, nrm(f3)@12, tex(f2)@24
// Push constants: { model: f4x4, viewProj: f4x4 } (128 bytes total)
VkResult CreateScenePipeline(VkDevice device, VkFormat colorFormat, VkFormat depthFormat,
                             FScenePipeline* OutPipeline);

void DestroyScenePipeline(VkDevice device, FScenePipeline Pipeline);

#endif