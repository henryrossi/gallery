#include "glyph.h"
#include "vulkan/vulkan_core.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/stat.h>

typedef struct {
        char *buf;
        uint32_t size;
} shader_bytecode;

// Reads in shader bytecode from file. Return 1 on success, 0 on failure.
static int read_shader_file(const char *filename, shader_bytecode s) {
        FILE *fp = fopen(filename, "rb");
        if (fp == NULL) {
                fprintf(stderr, "Failed to open file: %s.\n", filename);
                return 0;
        }

        size_t res = fread(s.buf, s.size, 1, fp);
        fclose(fp);

        if (res != 1) {
                fprintf(stderr, "Failed to read in file: %s.\n", filename);
                return 0;
        }

        return 1;
}

static shader_bytecode alloc_shader_buffer(const char *filename) {
        shader_bytecode res;

        struct stat s;
        stat(filename, &s);

        res.buf = malloc(sizeof(char) * s.st_size);
        res.size = s.st_size;
        return res;
}

// Creates shader module. Returns handle on success, VK_NULL_HANDLE on failure.
VkShaderModule create_shader_module(VkDevice device, const char *filename) {
        VkShaderModule res = VK_NULL_HANDLE;

        shader_bytecode code = alloc_shader_buffer(filename);
        if (read_shader_file(filename, code)) {
                VkShaderModuleCreateInfo createinfo = { 0 };
                createinfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                createinfo.codeSize = code.size;
                // When we do this cast we need to ensure the data satisfies the
                // alignment requirements of uint32_t.
                createinfo.pCode = (uint32_t *)code.buf;

                VkResult create_res
                    = vkCreateShaderModule(device, &createinfo, NULL, &res);
                if (create_res != VK_SUCCESS) {
                        fprintf(stderr, "Failed to create shader module: %s\n",
                                string_VkResult(create_res));
                }
        }

        free(code.buf);

        return res;
}
