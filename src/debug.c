#include "glyph.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

VkDebugUtilsMessengerEXT debug_messenger;

// Proxy functions for debug extension
static VkResult create_debug_utils_messenger_ext(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger) {
        PFN_vkCreateDebugUtilsMessengerEXT func
            = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != NULL) {
                return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        } else {
                return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
}

// Proxy function for debug extension
static void
destroy_debug_utils_messenger_ext(VkInstance instance,
                                  VkDebugUtilsMessengerEXT debugMessenger,
                                  const VkAllocationCallbacks *pAllocator) {
        PFN_vkDestroyDebugUtilsMessengerEXT func
            = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != NULL) {
                func(instance, debugMessenger, pAllocator);
        }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
               VkDebugUtilsMessageTypeFlagsEXT messageType,
               const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
               void *pUserData) {
        fprintf(stderr, "Validation Layer: %s\n", pCallbackData->pMessage);
        return VK_FALSE;
}

static void populate_debug_messenger_createinfo(
    VkDebugUtilsMessengerCreateInfoEXT *createinfo) {
        createinfo->sType
            = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createinfo->messageSeverity
            // = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
            = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
              | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createinfo->messageType
            = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
              | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
              | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createinfo->pfnUserCallback = debug_callback;
        createinfo->pUserData = NULL;
}

// Set up debug messenger. Returns 1 on success, 0 on failure
static int setup_debug_messenger(VkInstance instance) {
        VkDebugUtilsMessengerCreateInfoEXT createinfo = { 0 };
        populate_debug_messenger_createinfo(&createinfo);
        VkResult res = create_debug_utils_messenger_ext(instance, &createinfo,
                                                        NULL, &debug_messenger);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create debug messenger. %s\n",
                        string_VkResult(res));
                return 0;
        }
        return 1;
}

// Checks that all requested validation layers are available.
// Returns 1 on success, 0 on failure.
static int check_validation_layer_support(void) {
        uint32_t available_count = 0;
        VkResult res
            = vkEnumerateInstanceLayerProperties(&available_count, NULL);
        if (res != VK_SUCCESS) {
                fprintf(stderr,
                        "Failed to enumerate instance layer props: %s\n",
                        string_VkResult(res));
                return 0;
        }

        VkLayerProperties *available
            = malloc(sizeof(VkLayerProperties) * available_count);
        vkEnumerateInstanceLayerProperties(&available_count, available);

        for (int i = 0; i < validation_layer_count; i++) {
                uint32_t layer_found = 0;
                const char *layer = validation_layers[i];
                for (int j = 0; j < available_count; j++) {
                        if (strcmp(layer, available[j].layerName) == 0) {
                                layer_found = 1;
                                break;
                        }
                }

                if (!layer_found) {
                        fprintf(stderr, "Validation layer %s unavailable\n",
                                layer);
                        return 0;
                }
        }

        return 1;
}
