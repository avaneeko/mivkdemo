#include "vk.h"

#include <stdio.h>
#include <stdbool.h>

static VkResult create_vk_instance(VkInstance* instance)
{
    assert(instance);

    char const* extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
    uint32_t const extension_count = ARRAYSIZE(extensions);
    char const* enabled_layers[] = {"VK_LAYER_KHRONOS_validation"};
    uint32_t const enabled_layer_count = ARRAYSIZE(enabled_layers);

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = 0,
        .pApplicationName = "camellia",
        .applicationVersion = 1,
        .pEngineName = "camellia",
        .engineVersion = 1,
        .apiVersion = VK_API_VERSION_1_3,
    };

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = enabled_layer_count,
        .ppEnabledLayerNames = enabled_layers,
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames = extensions,
    };

    /* Create the instance. */
    VkResult res;
    VkInstance vk_instance;
    res = vkCreateInstance(&create_info, NULL, &vk_instance);

    if(res != VK_SUCCESS)
        return res;

    *instance = vk_instance;

    return VK_SUCCESS;
}

static VkResult choose_physical_device(VkInstance instance, VkPhysicalDevice* device)
{
    assert(device);

    uint32_t device_count = 0;
    VkResult res = vkEnumeratePhysicalDevices(instance, &device_count, NULL);

    if(res != VK_SUCCESS)
        return res;

    VkPhysicalDevice* devices = (VkPhysicalDevice*)malloc(device_count * sizeof(VkPhysicalDevice));

    if(!devices)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    res = vkEnumeratePhysicalDevices(instance, &device_count, devices);

    if(res != VK_SUCCESS && device_count != 0)
        return res;


    VkPhysicalDevice chosen_device = 0;
    for (size_t i = 0; i < device_count; ++i) {
        VkPhysicalDevice dev = devices[i];

        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceFeatures features;

        vkGetPhysicalDeviceProperties(dev, &properties);
        vkGetPhysicalDeviceFeatures(dev, &features);

        if(properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            /* TODO: Check for swapchain support? */
            /* TODO: Check swapchain present modes? (Has to have at least one.) */

            chosen_device = dev;
            break;
        }
    }

    free(devices);

    if(!chosen_device) {
        /* TODO; U BROKE BUY A REAL GPU */
        return VK_ERROR_UNKNOWN;
    }

    *device = chosen_device;

    return VK_SUCCESS;
}

/* Create logical device, ask for 1.2 and 1.3 vk features and a swapchain extension. */
VkResult create_logical_device(VkPhysicalDevice physical_device, uint32_t queue_index, VkDevice* new_device)
{
    assert(new_device);

    const float default_queue_priority = 0.5f;

    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .queueFamilyIndex = queue_index,
        .queueCount = 1,
        .pQueuePriorities = &default_queue_priority
    };

    const char* const enabled_extension_names[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkPhysicalDeviceVulkan13Features features13 = {0};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13.dynamicRendering = true;
	features13.synchronization2 = true;

    VkPhysicalDeviceVulkan12Features features12 = {0};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = &features13,
    features12.bufferDeviceAddress = true;
	features12.descriptorIndexing = true;

	VkPhysicalDeviceVulkan11Features features11 = {0};
	features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
	features11.shaderDrawParameters = true;
	features11.pNext = &features12;

    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features11,
        .flags = 0,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = ARRAYSIZE(enabled_extension_names),
        .ppEnabledExtensionNames = enabled_extension_names,
        .pEnabledFeatures = 0,
    };

    VkDevice device;
    VkResult res = vkCreateDevice(physical_device, &create_info, NULL, &device);

    if(res != VK_SUCCESS)
        return res;

    *new_device = device;

    return VK_SUCCESS;
}

/* Find a suitable queue that can do graphics and present. */
static VkResult find_queue(VkPhysicalDevice physical_device, VkSurfaceKHR surface, uint32_t* queue)
{
    assert(queue);

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, 0);

    VkQueueFamilyProperties* queue_families = (VkQueueFamilyProperties*)malloc(count * sizeof(VkQueueFamilyProperties));

    if(!queue_families)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, queue_families);

    uint32_t chosen_queue = 0xFFFFFFFF;
    for (size_t i = 0; i < count; ++i) {
        VkQueueFamilyProperties family = queue_families[i];

        if(family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            VkBool32 supports_present = VK_FALSE;

            VkResult res = vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, /* Queue family index */ i, surface, &supports_present);
            if(res == VK_SUCCESS && supports_present == VK_TRUE)
            {
                chosen_queue = i; /* Queue family index */
                break;
            }
        }
    }

    free(queue_families);

    if(chosen_queue == 0xFFFFFFFF) {
        /* Did not find the queue. */
        return VK_ERROR_UNKNOWN;
    }

    *queue = chosen_queue;

    return VK_SUCCESS;
}

/* Get device queue from the logical device */
static void get_queue_from_device(VkDevice device, uint32_t queue_index, VkQueue *queue)
{
    assert(queue);

    vkGetDeviceQueue(device, queue_index, 0, queue);
}

/* Create a Win32 KHR Surface. */
static VkResult create_surface(VkInstance instance, HWND hwnd, VkSurfaceKHR* surface)
{
    const VkWin32SurfaceCreateInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = 0,
        .flags = 0,
        .hinstance = GetModuleHandleA(0),
        .hwnd = hwnd,
    };

    VkSurfaceKHR new_surface;

    VkResult res = vkCreateWin32SurfaceKHR(instance, &info, 0, &new_surface);

    if(res != VK_SUCCESS)
        return res;

    *surface = new_surface;

    return VK_SUCCESS;
}

static VkFormat choose_swapchain_format(VkPhysicalDevice physical_device, VkSurfaceKHR surface)
{
    /* Get surface formats */
    VkSurfaceFormatKHR* formats;
    uint32_t format_count;
    {
        VkResult res = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, 0);
        if(res != VK_SUCCESS || format_count == 0) {
            return VK_FORMAT_UNDEFINED;
        }

        formats = (VkSurfaceFormatKHR*)malloc(format_count * sizeof(VkSurfaceFormatKHR));
        if(!formats) {
            return VK_FORMAT_UNDEFINED;
        }

        res = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, formats);
        if(res != VK_SUCCESS) {
            free(formats);
            return VK_FORMAT_UNDEFINED;
        }
    }

    VkFormat best_match = VK_FORMAT_UNDEFINED;
    VkFormat suitable[] = {VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM};

    for (size_t i = 0; i < format_count && best_match == VK_FORMAT_UNDEFINED; i++) {
        VkFormat fmt = formats[i].format;

        for (size_t k = 0; k < ARRAYSIZE(suitable); k++) {
            VkFormat suitable_fmt = suitable[k];

            if(fmt == suitable_fmt) {
                best_match = fmt;

                break;
            }
        }
    }

    return best_match;
}

static VkResult create_swapchain(VkDevice device, VkPhysicalDevice physical_device, unsigned short width, unsigned short height, VkSurfaceKHR surface, uint32_t queue_index, VkFormat* surface_format, VkExtent2D* out_window_size, VkSwapchainKHR* swapchain)
{
    assert(swapchain);
    assert(surface_format);

    VkFormat format = choose_swapchain_format(physical_device, surface);

    // We're using capabilities.currentExtent instead of width and height passed as it is for some reason incorrect.
    // It is not apparent why, but it seems that the driver is enforcing some specific size or the window size
    // calculation is not correct at the state of the window creation.
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities);
    // __builtin_dump_struct(&capabilities.currentExtent, printf);

    VkSwapchainCreateInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = 0,
        .flags = 0,
        .surface = surface,
        .minImageCount = capabilities.minImageCount, // Validation layer was complaining about the + 1.
        .imageFormat = format,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = capabilities.currentExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = NULL,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = NULL,
    };

    VkSwapchainKHR swap;
    VkResult res = vkCreateSwapchainKHR(device, &info, 0, &swap);

    if(res != VK_SUCCESS)
        return res;

    *swapchain = swap;
    *surface_format = format;
    out_window_size->width = capabilities.currentExtent.width;
    out_window_size->height = capabilities.currentExtent.height;

    return VK_SUCCESS;
}

static VkResult get_swapchain_images(VkDevice device, VkSwapchainKHR swapchain, VkImage** images, uint32_t* image_count)
{
    uint32_t count = 0;
    VkResult res = vkGetSwapchainImagesKHR(device, swapchain, &count, 0);

    if(res != VK_SUCCESS)
        return res;

    VkImage* imgs = (VkImage*)malloc(count * sizeof(VkImage));

    if(!imgs)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    res = vkGetSwapchainImagesKHR(device, swapchain, &count, imgs);

    if(res != VK_SUCCESS) {
        free(imgs);
        return res;
    }

    *images = imgs;
    *image_count = count;

    return VK_SUCCESS;
}

static VkResult create_image_views(VkDevice device, VkImage* images, uint32_t image_count, VkFormat format, VkImageView** out_image_views, uint32_t* out_image_view_count)
{
    assert(images);
    assert(out_image_views);
    assert(out_image_view_count);

    VkImageView* image_views = malloc(image_count * sizeof(VkImageView));

    for (size_t i = 0; i < image_count; i++) {
        const VkImageViewCreateInfo image_view_create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = 0,
            .flags = 0,
            .image = images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .components.r = VK_COMPONENT_SWIZZLE_R,
            .components.g = VK_COMPONENT_SWIZZLE_G,
            .components.b = VK_COMPONENT_SWIZZLE_B,
            .components.a = VK_COMPONENT_SWIZZLE_A,
            .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .subresourceRange.baseMipLevel = 0,
            .subresourceRange.levelCount = 1,
            .subresourceRange.baseArrayLayer = 0,
            .subresourceRange.layerCount = 1,
        };

        VkResult res = vkCreateImageView(device, &image_view_create_info, 0, &image_views[i]);

        if(res != VK_SUCCESS) {
            for (size_t k = 0; k < i; k++) {
                vkDestroyImageView(device, image_views[k], 0);
            }

            free(image_views);
            return res;
        }
    }

    *out_image_views = image_views;
    *out_image_view_count = image_count;

    return VK_SUCCESS;
}

uint32_t find_memory_type(VkPhysicalDevice physical_device, uint32_t memory_type_bits, VkMemoryPropertyFlags required_properties)
{
    VkPhysicalDeviceMemoryProperties prop;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &prop);

    for(uint32_t i = 0; i < prop.memoryTypeCount; i++) {
        bool is_type_compat = memory_type_bits & (1 << i);
        bool has_properties = (prop.memoryTypes[i].propertyFlags & required_properties);

        if (is_type_compat && has_properties)
            return i;
    }

    return 0xFFFFFFFF; // Error
}

static VkResult create_depth_buffer(VkDevice device, VkPhysicalDevice physical_device, VkExtent2D window_size, VkImage* depth_buffer, VkImageView* depth_buffer_view)
{
    assert(depth_buffer);
    assert(depth_buffer_view);

    const VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_D16_UNORM, // Apparently AMD does not support VK_FORMAT_X8_D24_UNORM_PACK32 at all.
        .extent = (VkExtent3D){.width = window_size.width, .height = window_size.height, .depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT, // Ideally it would be nice to support 4x MSAA, but that's for later.
        .tiling = VK_IMAGE_TILING_OPTIMAL, //TODO; this needs to be checked
        .usage =  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = 0,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VkResult res = vkCreateImage(device, &image_info, 0, depth_buffer);

    if(res != VK_SUCCESS)
        return res;

    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(device, *depth_buffer, &mem_req);

    uint32_t memory_type_index = find_memory_type(physical_device, mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(memory_type_index == 0xFFFFFFFF) {
        /* Couldn't find an appropriate memory type index. */
        vkDestroyImage(device, *depth_buffer, 0);
        return VK_ERROR_UNKNOWN;
    }

    const VkMemoryAllocateInfo mem_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = 0,
        .allocationSize = mem_req.size,
        .memoryTypeIndex = memory_type_index,
    };

    VkDeviceMemory mem;
    res = vkAllocateMemory(device, &mem_info, 0, &mem);
    if(res != VK_SUCCESS) {
        vkDestroyImage(device, *depth_buffer, 0);
        return res;
    }

    res = vkBindImageMemory(device, *depth_buffer, mem, 0);
    if(res != VK_SUCCESS) {
        vkFreeMemory(device, mem, 0);
        vkDestroyImage(device, *depth_buffer, 0);
        return res;
    }

    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .image = *depth_buffer,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_D16_UNORM,
        .components = {.r = VK_COMPONENT_SWIZZLE_R, .g = VK_COMPONENT_SWIZZLE_G, .b = VK_COMPONENT_SWIZZLE_B, .a = VK_COMPONENT_SWIZZLE_A},
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };

    res = vkCreateImageView(device, &view_info, 0, depth_buffer_view);
    if(res != VK_SUCCESS) {
        vkFreeMemory(device, mem, 0);
        vkDestroyImage(device, *depth_buffer, 0);
        return res;
    }

    return VK_SUCCESS;
}


/* Create 2 sets of command pools for double buffering the commands. */
static VkResult create_command_pool(VkDevice device, VkQueue queue, uint32_t queue_index, VkCommandPool* command_pools, VkCommandBuffer* command_buffers)
{
    assert(command_pools);
    assert(command_buffers);

    const VkCommandPoolCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = 0,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queue_index,
    };

    /* Create two pairs for double buffering. */
    for (size_t i = 0; i < 2; i++) {
        VkResult res = vkCreateCommandPool(device, &info, 0, &command_pools[i]);
        if(res != VK_SUCCESS)
            return res;

        const VkCommandBufferAllocateInfo cmd_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = 0,
            .commandPool = command_pools[i],
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        res = vkAllocateCommandBuffers(device, &cmd_info, &command_buffers[i]);
        if(res != VK_SUCCESS)
            return res;
    }

    return VK_SUCCESS;
}

static VkResult create_sync(VkDevice device, VkFence* render_fences, VkSemaphore* swapchain_semaphores, VkSemaphore* render_semaphores)
{
    assert(render_fences);
    assert(swapchain_semaphores);
    assert(render_semaphores);

    const VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = 0,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    const VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
    };

    // 2 because of double-buffering.
    for (size_t i = 0; i < 2; i++) {
        VkResult res = vkCreateFence(device, &fence_info, 0, &render_fences[i]);

        if(res != VK_SUCCESS)
            return res;

        res = vkCreateSemaphore(device, &semaphore_info, 0, &swapchain_semaphores[i]);

        if(res != VK_SUCCESS)
            return res;

        res = vkCreateSemaphore(device, &semaphore_info, 0, &render_semaphores[i]);

        if(res != VK_SUCCESS)
            return res;
    }

    return VK_SUCCESS;
}

/* Initialize vulkan. */
VkResult vk_init(vulkan* vk, HWND hwnd, unsigned short width, unsigned short height)
{
    assert(vk);

    /* Create vk instance. */
    VkResult res = create_vk_instance(&vk->instance);

    if(res != VK_SUCCESS)
        return res;

    /* Choose physical device. */
    res = choose_physical_device(vk->instance, &vk->physical_device);

    if(res != VK_SUCCESS)
        return res;

    /* Create a surface. */
    res = create_surface(vk->instance, hwnd, &vk->surface);

    if(res != VK_SUCCESS)
        return res;

    /* Find the queue */
    res = find_queue(vk->physical_device, vk->surface, &vk->queue_index);

    if(res != VK_SUCCESS)
        return res;

    /* Create a logical device. */
    res = create_logical_device(vk->physical_device, vk->queue_index, &vk->device);

    if(res != VK_SUCCESS)
        return res;

    /* Get the queue. */
    get_queue_from_device(vk->device, vk->queue_index, &vk->queue);

    /* Create the swapchain. */
    res = create_swapchain(vk->device, vk->physical_device, width, height, vk->surface, vk->queue_index, &vk->surface_format, &vk->window_size, &vk->swapchain);

    if(res != VK_SUCCESS)
        return res;

     /* Get swapchain's images. */
    res = get_swapchain_images(vk->device, vk->swapchain, &vk->swapchain_images, &vk->swapchain_image_count);

    if(res != VK_SUCCESS)
        return res;

    /* Create image views for the swapchain images. */
    res = create_image_views(vk->device, vk->swapchain_images, vk->swapchain_image_count, vk->surface_format, &vk->swapchain_image_views, &vk->swapchain_image_view_count);

    if(res != VK_SUCCESS)
        return res;

    /* Create and allocate the depth buffer. */
    res = create_depth_buffer(vk->device, vk->physical_device, vk->window_size, &vk->depth_buffer, &vk->depth_buffer_view);

    if(res != VK_SUCCESS)
        return res;

    /* Create command pool. */
    res = create_command_pool(vk->device, vk->queue, vk->queue_index, vk->command_pools, vk->command_buffers);

    if(res != VK_SUCCESS)
        return res;

    /* Create sync primitives. */
    res = create_sync(vk->device, vk->render_fences, vk->swapchain_semaphores, vk->render_semaphores);

    return res;
}
