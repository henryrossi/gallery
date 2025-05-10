#include "glyph.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "debug.c"
#include "vulkan/vk_platform.h"

const uint32_t width = 800;
const uint32_t height = 600;

static VkInstance instance;
static VkDevice device;
static VkPhysicalDevice physical_device = VK_NULL_HANDLE;
static VkQueue graphics_queue;
static VkQueue presentation_queue;
static GLFWwindow *window;
static VkSurfaceKHR surface = VK_NULL_HANDLE;

static const char *device_exts[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        "VK_KHR_portability_subset",
};

static GLFWwindow *init_window(void) {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        GLFWwindow *window
            = glfwCreateWindow(width, height, "glyph", NULL, NULL);
        return window;
}

// Creates a surface for the WSI (window system integratoin).
// Returns 1 on success, 0 on failure.
static int create_surface(void) {
        VkResult res
            = glfwCreateWindowSurface(instance, window, NULL, &surface);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create surface: %s\n",
                        string_VkResult(res));
                return 0;
        }
        return 1;
}

typedef struct {
        uint32_t index;
        uint32_t valid;
} queue_family_index_t;

typedef struct {
        queue_family_index_t graphics;
        queue_family_index_t presentation;
} queue_family_indicies_t;

static queue_family_indicies_t find_queue_families(VkPhysicalDevice device) {
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
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface,
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
        VkSurfaceCapabilitiesKHR capabilities;
        VkSurfaceFormatKHR *formats;
        VkPresentModeKHR *present_modes;
        uint32_t formats_count;
        uint32_t present_modes_count;
} swapchain_support_details;

swapchain_support_details query_swapchain_support(VkPhysicalDevice device) {
        swapchain_support_details details = { 0 };

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
                                                  &details.capabilities);

        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface,
                                             &details.formats_count, NULL);
        details.formats
            = malloc(sizeof(VkSurfaceFormatKHR) * details.formats_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            device, surface, &details.formats_count, details.formats);

        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device, surface, &details.present_modes_count, NULL);
        details.present_modes
            = malloc(sizeof(VkPresentModeKHR) * details.present_modes_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface,
                                                  &details.present_modes_count,
                                                  details.present_modes);

        return details;
}

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

VkSurfaceFormatKHR choose_sc_surface_format(swapchain_support_details details) {
        VkSurfaceFormatKHR format;
        return format;
}

// Determines if a physical device is suitable for our needs.
// Returns 1 if suitable, 0 if unsuitable.
static int is_device_suitable(VkPhysicalDevice device) {
        VkPhysicalDeviceFeatures feats;
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceFeatures(device, &feats);
        vkGetPhysicalDeviceProperties(device, &props);

        queue_family_indicies_t indicies = find_queue_families(device);

        int extensions_supported = check_device_extension_support(device);

        int swapchain_adequate = 0;
        if (extensions_supported) {
                swapchain_support_details sc_support
                    = query_swapchain_support(device);
                swapchain_adequate = sc_support.formats_count
                                     && sc_support.present_modes_count;
        }

        return indicies.graphics.valid && indicies.presentation.valid
               && extensions_supported && swapchain_adequate;
}

// Pick physical device to use. Returns 1 on succces, 0 on failure.
static int pick_physical_device(void) {
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(instance, &device_count, NULL);

        if (device_count == 0) {
                fprintf(stderr, "No physical devices found\n");
                return 0;
        }

        VkPhysicalDevice *devices
            = malloc(sizeof(VkPhysicalDevice) * device_count);
        vkEnumeratePhysicalDevices(instance, &device_count, devices);

        for (uint32_t i = 0; i < device_count; i++) {
                VkPhysicalDevice device = devices[i];
                if (is_device_suitable(device)) {
                        physical_device = device;
                        break;
                }
        }

        if (physical_device == VK_NULL_HANDLE) {
                fprintf(stderr, "Failed to find a suitable physical device\n");
                return 0;
        }

        return 1;
}

// Creates logical device. Returns 1 on success, 0 on failure.
static int create_logical_device(void) {
        queue_family_indicies_t indicies = find_queue_families(physical_device);
        float queue_priority = 1.0f;

        if (!indicies.graphics.valid || !indicies.graphics.valid) {
                fprintf(stderr,
                        "Device doesn't support required queue families\n");
                return 0;
        }

        uint32_t q_createinfo_count = 1;
        VkDeviceQueueCreateInfo q_createinfo[2] = { 0 };
        VkDeviceQueueCreateInfo *q_graphics_createinfo = q_createinfo;
        q_graphics_createinfo->sType
            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        q_graphics_createinfo->queueFamilyIndex = indicies.graphics.index;
        q_graphics_createinfo->queueCount = 1;
        q_graphics_createinfo->pQueuePriorities = &queue_priority;

        // if required queues happen to be in the same queue family we must only
        // create one queue per family
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

        VkPhysicalDeviceFeatures features = { 0 };

        VkDeviceCreateInfo createinfo = { 0 };
        createinfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createinfo.pQueueCreateInfos = q_createinfo;
        createinfo.queueCreateInfoCount = q_createinfo_count;
        createinfo.pEnabledFeatures = &features;

        createinfo.enabledExtensionCount
            = sizeof(device_exts) / sizeof(device_exts[0]);
        createinfo.ppEnabledExtensionNames = device_exts;

        if (enable_validation_layers) {
                createinfo.enabledLayerCount = validation_layer_count;
                createinfo.ppEnabledLayerNames = validation_layers;
        } else {
                createinfo.enabledLayerCount = 0;
        }

        VkResult res
            = vkCreateDevice(physical_device, &createinfo, NULL, &device);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create device, %s\n",
                        string_VkResult(res));
                return 0;
        }

        vkGetDeviceQueue(device, indicies.graphics.index, 0, &graphics_queue);
        vkGetDeviceQueue(device, indicies.presentation.index, 0,
                         &presentation_queue);

        return 1;
}

typedef struct {
        const char **names;
        uint32_t count;
} extensions_t;

static extensions_t get_required_extensions(void) {
        extensions_t exts = { 0 };

        // The following is useful for checking the existence of an extension
        // uint32_t supported_ext_count = 0;
        // vkEnumerateInstanceExtensionProperties(NULL, &supported_ext_count,
        //                                        NULL);
        // VkExtensionProperties *supported_exts
        //     = malloc(sizeof(VkExtensionProperties) * supported_ext_count);
        // vkEnumerateInstanceExtensionProperties(NULL, &supported_ext_count,
        //                                        supported_exts);
        // printf("Available supported extensions:\n");
        // for (uint32_t i = 0; i < supported_ext_count; i++) {
        //         printf("  %s\n", supported_exts[i].extensionName);
        // }
        // free(supported_exts);

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
static int create_instance(void) {
        VkApplicationInfo appinfo = { 0 };
        appinfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appinfo.pApplicationName = "glyph";
        appinfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appinfo.pEngineName = "No Engine";
        appinfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appinfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createinfo = { 0 };
        createinfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createinfo.pApplicationInfo = &appinfo;

        extensions_t exts = get_required_extensions();
        createinfo.enabledExtensionCount = exts.count;
        createinfo.ppEnabledExtensionNames = exts.names;
        createinfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

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

        VkResult res = vkCreateInstance(&createinfo, NULL, &instance);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create instance. %s\n",
                        string_VkResult(res));
                return 0;
        }
        return 1;
}

int main(int argc, char **argv) {
        window = init_window();

        if (!create_instance()) {
                return 1;
        }
        if (!create_surface()) {
                return 1;
        }
        if (enable_validation_layers && !setup_debug_messenger(instance)) {
                return 1;
        }
        if (!pick_physical_device()) {
                return 1;
        }
        if (!create_logical_device()) {
                return 1;
        }

        while (!glfwWindowShouldClose(window)) {
                glfwPollEvents();
        }

        if (enable_validation_layers) {
                destroy_debug_utils_messenger_ext(instance, debug_messenger,
                                                  NULL);
        }
        vkDestroyDevice(device, NULL);
        vkDestroySurfaceKHR(instance, surface, NULL);
        vkDestroyInstance(instance, NULL);
        glfwDestroyWindow(window);

        glfwTerminate();
        return 0;
}
