#include "engine.h"
#include "glyph.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *device_exts[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        "VK_KHR_portability_subset",
};

// Check that a device supports all required extensions.
// Returns 1 if all extensions supported, 0 if not.
static int check_device_extension_support(VkPhysicalDevice device) {
        uint32_t available_count = 0;
        vkEnumerateDeviceExtensionProperties(device, NULL, &available_count,
                                             NULL);

        VkExtensionProperties *available
            = malloc(sizeof(VkExtensionProperties) * available_count);
        vkEnumerateDeviceExtensionProperties(device, NULL, &available_count,
                                             available);

        uint32_t required_count = sizeof(device_exts) / sizeof(device_exts[0]);

        for (uint32_t i = 0; i < required_count; i++) {
                const char *ext = device_exts[i];
                int found = 0;
                for (uint32_t j = 0; j < available_count; j++) {
                        if (strcmp(ext, available[j].extensionName) == 0) {
                                found = 1;
                                break;
                        }
                }

                if (!found) {
                        free(available);
                        return 0;
                }
        }

        free(available);

        return 1;
}

// Determines if a physical device is suitable for our needs.
// Returns 1 if suitable, 0 if unsuitable.
static int is_device_suitable(GlyphEngine *engine, VkPhysicalDevice device) {
        VkPhysicalDeviceFeatures feats;
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceFeatures(device, &feats);
        vkGetPhysicalDeviceProperties(device, &props);

        queue_family_indicies_t indicies = find_queue_families(engine, device);

        int extensions_supported = check_device_extension_support(device);

        int swapchain_adequate = 0;
        if (extensions_supported) {
                swapchain_support_details_t sc_support
                    = query_swapchain_support(engine, device);
                swapchain_adequate = sc_support.formats_count
                                     && sc_support.present_modes_count;
                free(sc_support.formats);
                free(sc_support.present_modes);
        }

        return indicies.graphics.valid && indicies.presentation.valid
               && extensions_supported && swapchain_adequate;
}

// Pick physical device to use. Returns 1 on succces, 0 on failure.
static int pick_physical_device(GlyphEngine *engine) {
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(engine->instance, &device_count, NULL);

        if (device_count == 0) {
                fprintf(stderr, "No physical devices found\n");
                return 0;
        }

        VkPhysicalDevice *devices
            = malloc(sizeof(VkPhysicalDevice) * device_count);
        vkEnumeratePhysicalDevices(engine->instance, &device_count, devices);

        for (uint32_t i = 0; i < device_count; i++) {
                VkPhysicalDevice device = devices[i];
                if (is_device_suitable(engine, device)) {
                        engine->physical_device = device;
                        break;
                }
        }

        if (engine->physical_device == VK_NULL_HANDLE) {
                fprintf(stderr, "Failed to find a suitable physical device\n");
                return 0;
        }

        return 1;
}

// Creates logical device. Returns 1 on success, 0 on failure.
static int create_logical_device(GlyphEngine *engine) {
        queue_family_indicies_t indicies
            = find_queue_families(engine, engine->physical_device);
        float queue_priority = 1.0f;

        if (!indicies.graphics.valid || !indicies.graphics.valid) {
                fprintf(stderr,
                        "Device doesn't support required queue families\n");
                return 0;
        }

        uint32_t q_createinfo_count = 1;
        VkDeviceQueueCreateInfo q_createinfo[2] = {
                {
                        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                        .queueFamilyIndex = indicies.graphics.index,
                        .queueCount = 1,
                        .pQueuePriorities = &queue_priority,
                },
        };

        // if required queues happen to be in the same queue family we
        // must only create one queue per family
        if (indicies.graphics.index != indicies.presentation.index) {
                q_createinfo_count++;
                VkDeviceQueueCreateInfo *q_present_createinfo
                    = q_createinfo + 1;
                q_present_createinfo->sType
                    = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                q_present_createinfo->queueFamilyIndex
                    = indicies.presentation.index;
                q_present_createinfo->queueCount = 1;
                q_present_createinfo->pQueuePriorities = &queue_priority;
        }

        VkDeviceCreateInfo createinfo = {
                .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .pQueueCreateInfos = q_createinfo,
                .queueCreateInfoCount = q_createinfo_count,
                .pEnabledFeatures = &features,
                .enabledExtensionCount
                = sizeof(device_exts) / sizeof(device_exts[0]),
                .ppEnabledExtensionNames = device_exts,
        };

        if (enable_validation_layers) {
                createinfo.enabledLayerCount = validation_layer_count;
                createinfo.ppEnabledLayerNames = validation_layers;
        }

        VkResult res = vkCreateDevice(engine->physical_device, &createinfo,
                                      NULL, &engine->device);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create device, %s\n",
                        string_VkResult(res));
                return 0;
        }

        vkGetDeviceQueue(engine->device, indicies.graphics.index, 0,
                         &engine->graphics_queue);
        vkGetDeviceQueue(engine->device, indicies.presentation.index, 0,
                         &engine->presentation_queue);

        return 1;
}
