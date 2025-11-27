#include "engine.h"

#include <stdlib.h>

#include "debug.c"

static queue_family_indicies_t find_queue_families(GlyphEngine *engine,
                                                   VkPhysicalDevice device) {
        queue_family_indicies_t indicies = { 0 };

        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                                 NULL);
        VkQueueFamilyProperties *queue_families
            = malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                                 queue_families);

        for (uint32_t i = 0; i < queue_family_count; i++) {
                if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                        indicies.graphics.index = i;
                        indicies.graphics.valid = 1;
                }
                uint32_t supports_presentation = 0;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, engine->surface,
                                                     &supports_presentation);
                if (supports_presentation) {
                        indicies.presentation.index = i;
                        indicies.presentation.valid = 1;
                }
        }

        free(queue_families);

        return indicies;
}

typedef struct {
        const char **names;
        uint32_t count;
} extensions_t;

static extensions_t get_required_extensions(void) {
        extensions_t exts = { 0 };

        uint32_t glfw_ext_count = 0;
        const char **glfw_exts
            = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

        exts.count = enable_validation_layers ? glfw_ext_count + 3
                                              : glfw_ext_count + 2;
        exts.names = malloc(sizeof(const char *) * exts.count);

        for (int i = 0; i < glfw_ext_count; i++) {
                exts.names[i] = glfw_exts[i];
        }

        if (enable_validation_layers) {
                exts.names[exts.count - 3] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        }
        exts.names[exts.count - 2]
            = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
        exts.names[exts.count - 1] = "VK_KHR_get_physical_device_properties2";

        return exts;
}

// Creates a Vulkan instance. Returns 1 on success, 0 on error.
static int create_instance(GlyphEngine *engine) {
        VkApplicationInfo appinfo = {
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pApplicationName = "glyph",
                .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                .pEngineName = "No Engine",
                .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                .apiVersion = VK_API_VERSION_1_0,
        };

        extensions_t exts = get_required_extensions();
        VkInstanceCreateInfo createinfo = {
                .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                .pApplicationInfo = &appinfo,
                .enabledExtensionCount = exts.count,
                .ppEnabledExtensionNames = exts.names,
                .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
        };

        VkDebugUtilsMessengerCreateInfoEXT debug_createinfo = { 0 };
        if (enable_validation_layers) {
                if (!check_validation_layer_support()) {
                        return 0;
                }
                createinfo.enabledLayerCount = validation_layer_count;
                createinfo.ppEnabledLayerNames = validation_layers;
                populate_debug_messenger_createinfo(&debug_createinfo);
                createinfo.pNext = &debug_createinfo;
        } else {
                createinfo.enabledLayerCount = 0;
        }

        VkResult res = vkCreateInstance(&createinfo, NULL, &engine->instance);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create instance. %s\n",
                        string_VkResult(res));
                return 0;
        }
        return 1;
}
