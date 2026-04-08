#include "load_shader_module.h"
#include <stdio.h>
#include <stdlib.h>

VkResult load_shader_module(VkDevice dev, char const* path, VkShaderModule* out)
{
    FILE* file = fopen(path, "rb");

    if (!file)
    {
        printf("Fatal error: Failed to a load shader module.\r\nCannot open file \"%s\".", path);
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    uint64_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    void* data = malloc(size);
    if (!data)
    {
        printf("Fatal error: Failed to a load shader module.\r\nOut of memory loading \"%s\".", path);
        exit(1);
    }
    fread(data, size, 1, file);
    fclose(file);

    VkShaderModuleCreateInfo const info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .codeSize = size,
        .pCode = data,
    };

    VkResult const res = vkCreateShaderModule(dev, &info, NULL, out);
    free(data);

    return res;
}
