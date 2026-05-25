#include "Core.hpp"
#include <stdlib.h>
#include <windows.h>
#include <stdio.h>

#include "SceneLoader.hpp"
#include "tri_pipeline.h"
#include "upload_gpu_data.h"
#include "window.h"
#include "vk.h"

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

        float time = (float)(GetTickCount() % 256) / 255.0f;  // Cycles every 256 frames
        VkClearValue clearColor = { {{ time, 0.0f, 1.0f - time, 1.0f }} };  // RGB shifts over time

        res = vkResetCommandBuffer(cmd, 0);
        assert(res == VK_SUCCESS);

        res = vkBeginCommandBuffer(cmd, &info);
        assert(res == VK_SUCCESS);
        /* Rendering command recording start. */

        VkImageMemoryBarrier2 barrier_to_color = {
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
        };

        VkDependencyInfo dep_to_color = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrier_to_color,
        };
        vkCmdPipelineBarrier2(cmd, &dep_to_color);

        VkRenderingAttachmentInfo color_attachment = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = vk.swapchain_image_views[swapchain_image_index],
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = {.color = {0.0f, 0.0f, 0.0f, 1.0f}},
        };

        VkRenderingInfo rendering_info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = {.extent = vk.window_size},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_attachment,
        };

        vkCmdBeginRendering(cmd, &rendering_info);

        VkViewport viewport = {
            .width = (float)vk.window_size.width,
            .height = (float)vk.window_size.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor = {.extent = vk.window_size};
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, tri_pipeline);
        VkDeviceSize Offsets[1] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer, Offsets);
        vkCmdDraw(cmd, 3, 1, 0, 0);

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

        Sleep(1);
    }

    return 0;
}
