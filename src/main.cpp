#include "Core.hpp"
#include <stdlib.h>
#include <windows.h>
#include <stdio.h>

#include "SceneLoader.hpp"
#include "ScenePipeline.hpp"
#include "tri_pipeline.h"
#include "upload_gpu_data.h"
#include "window.h"
#include "vk.h"

#include <cmath>
#include <cstring>
#include "FMat4.hpp"

typedef struct {
    VkCommandPool cmd_pool;
    VkCommandBuffer cmd_buf;
    VkFence render_fence;
    VkSemaphore swapchain_semaphore;
    VkSemaphore render_semaphore;
} frame_t;

static uint8_t cur_frame = 0;
static void get_frame(const vulkan* vk, frame_t* frame)
{
    frame->cmd_pool = vk->command_pools[cur_frame];
    frame->cmd_buf = vk->command_buffers[cur_frame];
    frame->render_fence = vk->render_fences[cur_frame];
    frame->swapchain_semaphore = vk->swapchain_semaphores[cur_frame];
    frame->render_semaphore = vk->render_semaphores[cur_frame];

    if(cur_frame == 0)
        cur_frame = 1;
    else
        cur_frame = 0;
}

int main(int argc, const char** argv)
{
    char const* TestSceneString;
    if (argc > 1)
    {
        TestSceneString = argv[1];
    }
    else
    {
        TestSceneString = getenv("TEST_GLB_FILEPATH");
    }

    if (not TestSceneString)
    {
        fprintf(stderr, "No GLTF scene file provided.\r\n Pass argv[1] or TEST_GLB_FILEPATH env var.\r\n");
    }

    SceneLoader Loader;
    std::vector<CScene> Scenes;
    {// Testing SceneLoader
        Loader.LoadScene(TestSceneString, Scenes);
        printf("Scene count: %zu\r\n", Scenes.size());
        for (size_t si = 0; si < Scenes.size(); si++) {
            printf("  Scene %zu: \"%s\" has %zu instance(s)\r\n",
                si, Scenes[si].Name.c_str(), Scenes[si].Instances.size());
        }
    }

    /* Create the window. */
    unsigned short width = 1280;
    unsigned short height = 720;

    Window window;
    {
        bool res = window_init(&window, "camellia", width, height);
        if(!res)
            exit(EXIT_FAILURE);
    }

    vulkan vk;
    VkPipeline tri_pipeline;
    VkPipelineLayout tri_pipeline_layout;
    {
        VkResult res = vk_init(&vk, window.hwnd, width, height);
        if(res != VK_SUCCESS)
            exit(EXIT_FAILURE);

        res = create_tri_pipeline(vk.device, vk.surface_format, VK_FORMAT_D16_UNORM, &tri_pipeline, &tri_pipeline_layout);
        if(res != VK_SUCCESS)
        {
            printf("Fatal error: Failed to create tri pipeline.\r\n");
            exit(1);
        }
        printf("Tri pipeline created!\r\n");
    }

    FScenePipeline ScenePipeline;
    {
        VkResult SceneRes = CreateScenePipeline(vk.device, vk.surface_format, VK_FORMAT_D16_UNORM, &ScenePipeline);
        if (SceneRes != VK_SUCCESS)
        {
            printf("Fatal error: Failed to create scene pipeline.\r\n");
            exit(1);
        }
        printf("Scene pipeline created!\r\n");
    }

    VkResult LoaderUploadResult =
        Loader.UploadMeshData(vk.device, vk.physical_device, vk.command_pools[0], vk.queue);
    check(LoaderUploadResult == VK_SUCCESS);

    // testing
    float const data[] =
    {
         0.0f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 67.0f,
         0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 67.0f,
        -0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 67.0f,
    };
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;
    // VkResult res = upload_gpu_data(vk.device, vk.physical_device, vk.command_pools[0], vk.queue, &vertex_buffer, &vertex_buffer_memory);
    VkResult res = create_vertex_buffer(vk.device, vk.physical_device, vk.command_pools[0], vk.queue, data, sizeof(data), &vertex_buffer, &vertex_buffer_memory);
    printf("Upload GPU Data result: %i\r\n", res);
    // end of testing.

    frame_t frame;

    //__builtin_dump_struct(&vk, printf);

    while (!windows_process_msg_queue()) {
        get_frame(&vk, &frame);
        VkCommandBuffer cmd = frame.cmd_buf;

        vkWaitForFences(vk.device, 1, &frame.render_fence, VK_TRUE, 1000000000);
        vkResetFences(vk.device, 1, &frame.render_fence);

        /* Request image from the swapchain. */
        uint32_t swapchain_image_index;
        VkResult res = vkAcquireNextImageKHR(vk.device, vk.swapchain, 1000000000, frame.swapchain_semaphore, 0, &swapchain_image_index);
        //printf("vkAcquireNextImageKHR failed: %d\n", res);
        assert(res == VK_SUCCESS);

        VkCommandBufferBeginInfo const info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = 0,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = 0,
        };

        res = vkResetCommandBuffer(cmd, 0);
        check(res == VK_SUCCESS);

        res = vkBeginCommandBuffer(cmd, &info);
        check(res == VK_SUCCESS);
        /* Rendering command recording start. */

        /* Barriers: transition color + depth to attachment optimal */
        VkImageMemoryBarrier2 barriers_to_attachments[2] = {
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext = NULL,
                .srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                .srcAccessMask = 0,
                .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .srcQueueFamilyIndex = 0,
                .dstQueueFamilyIndex = 0,
                .image = vk.swapchain_images[swapchain_image_index],
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            },
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext = NULL,
                .srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                .srcAccessMask = 0,
                .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                .dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                .srcQueueFamilyIndex = 0,
                .dstQueueFamilyIndex = 0,
                .image = vk.depth_buffer,
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            },
        };

        VkDependencyInfo dep_to_attachments = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 2,
            .pImageMemoryBarriers = barriers_to_attachments,
        };
        vkCmdPipelineBarrier2(cmd, &dep_to_attachments);

        VkClearValue clearColor = { .color = { 0.07f, 0.07f, 0.12f, 1.0f } };
        VkClearValue clearDepth = { .depthStencil = { 1.0f, 0 } };

        VkRenderingAttachmentInfo colorAttachment = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = vk.swapchain_image_views[swapchain_image_index],
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = clearColor,
        };

        VkRenderingAttachmentInfo depthAttachment = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = vk.depth_buffer_view,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .clearValue = clearDepth,
        };

        VkRenderingInfo renderingInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = { .extent = vk.window_size },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachment,
            .pDepthAttachment = &depthAttachment,
        };

        vkCmdBeginRendering(cmd, &renderingInfo);

        VkViewport viewport = {
            .width = (float)vk.window_size.width,
            .height = (float)vk.window_size.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor = { .extent = vk.window_size };
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // Cam
        float aspect = (float)vk.window_size.width / (float)vk.window_size.height;
        float timeSec = (float)GetTickCount() * 0.001f;
        float radius = 5.0f;
        float camAngle = timeSec * 0.3f;

        FVec3 eye = { radius * cosf(camAngle), radius * sinf(camAngle), 3.0f };
        FVec3 target = { 0.0f, 0.0f, 0.0f };
        FVec3 up = { 0.0f, 0.0f, 1.0f };

        float fovY = 1.0f; // ~57 degrees
        FMat4 proj = FMat4::Perspective(fovY, aspect, 0.1f, 1000.0f);
        FMat4 view = FMat4::LookAt(eye, target, up);

        // Draw scene
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ScenePipeline.Pipeline);

        auto const& meshes = Loader.GetMeshes();

        for (auto const& Scene : Scenes) {
            for (auto const& Inst : Scene.Instances) {
                auto const& mesh = meshes[Inst.MeshIndex];

                FMat4 model = FMat4::FromFloat16(Inst.WorldMatrix);
                FMat4 modelView = view * model;

                // Push constants: { modelView, proj } = 128 bytes
                struct { FMat4 modelView; FMat4 proj; } pushData;
                pushData.modelView = modelView;
                pushData.proj = proj;
                vkCmdPushConstants(cmd, ScenePipeline.Layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushData), &pushData);

                VkDeviceSize vbOffset = mesh.VertexBufferOffset;
                vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.VertexBuffer, &vbOffset);
                vkCmdBindIndexBuffer(cmd, mesh.IndexBuffer, mesh.IndexBufferOffset, mesh.IndexBufferType);

                uint32_t indexCount = (uint32_t)(mesh.IndexBufferSize / (mesh.IndexBufferType == VK_INDEX_TYPE_UINT16 ? sizeof(uint16_t) : sizeof(uint32_t)));
                vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
            }
        }

        vkCmdEndRendering(cmd);

        VkImageMemoryBarrier2 barrier_to_present = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
            .dstAccessMask = 0,
            .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .image = vk.swapchain_images[swapchain_image_index],
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };

        VkDependencyInfo dep_to_present = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrier_to_present,
        };
        vkCmdPipelineBarrier2(cmd, &dep_to_present);

        /* End of rendering command recording. */
        res = vkEndCommandBuffer(cmd);
        assert(res == VK_SUCCESS);


        /* Submission of the command buffer to the queue. */
        VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &frame.swapchain_semaphore,  // Synchronizing with swapchain
            .pWaitDstStageMask = wait_stages,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &frame.render_semaphore   // Signals that rendering is done
        };

        VkResult submitRes = vkQueueSubmit(vk.queue, 1, &submitInfo, frame.render_fence);
        if (submitRes != VK_SUCCESS) {
            printf("vkQueueSubmit failed: %i\r\n", submitRes);
            assert(0);
        }

        /* Presentation of the frame. */
        VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &frame.render_semaphore,  // Make sure it's signaled by vkQueueSubmit
            .swapchainCount = 1,
            .pSwapchains = &vk.swapchain,
            .pImageIndices = &swapchain_image_index
        };

        VkResult presentRes = vkQueuePresentKHR(vk.queue, &present_info);
        if (presentRes != VK_SUCCESS) {
            printf("vkQueuePresentKHR failed: %i\n", presentRes);
            assert(0);
        }

        // Sleep(1);
    }

    DestroyScenePipeline(vk.device, ScenePipeline);

    return 0;
}
