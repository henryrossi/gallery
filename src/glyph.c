#include "glyph.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "debug.c"
#include "shader.c"

#include "vulkan/vk_platform.h"
#include "vulkan/vulkan_core.h"

const uint32_t default_window_width = 800;
const uint32_t default_window_height = 600;

#define MAX_FRAMES_IN_FLIGHT 2
uint32_t current_frame = 0;

static VkInstance instance;
static VkDevice device;
static VkPhysicalDevice physical_device = VK_NULL_HANDLE;
static VkQueue graphics_queue;
static VkQueue presentation_queue;
static GLFWwindow *window;
static VkSurfaceKHR surface = VK_NULL_HANDLE;
static VkSwapchainKHR swapchain;
static uint32_t swapchain_images_count;
static VkImage *swapchain_images;
static VkFormat swapchain_format;
static VkExtent2D swapchain_extent;
static uint32_t swapchain_image_views_count;
static VkImageView *swapchain_image_views;
static uint32_t swapchain_framebuffer_count;
static VkFramebuffer *swapchain_framebuffers;
static VkRenderPass render_pass;
static VkPipelineLayout pipeline_layout;
static VkPipeline graphics_pipeline;
static VkCommandPool command_pool;
static VkCommandBuffer command_buffer[MAX_FRAMES_IN_FLIGHT];
static VkSemaphore image_available_semaphore[MAX_FRAMES_IN_FLIGHT];
static VkSemaphore render_finished_semaphore[MAX_FRAMES_IN_FLIGHT];
static VkFence inflight_fence[MAX_FRAMES_IN_FLIGHT];

static uint32_t framebuffer_resized = 0;

typedef struct {
        float pos[2];
        float color[3];
} vertex;

const vertex verticies[] = {
        { { 0.0f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
        { { 0.5f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
        { { -0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f } },
};

static const char *device_exts[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        "VK_KHR_portability_subset",
};

static void framebuffer_resize_callback(GLFWwindow *window, int width,
                                        int height) {
        // glfwGetWindowUserPointer
        framebuffer_resized = 1;
}

static GLFWwindow *init_window(void) {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        GLFWwindow *window = glfwCreateWindow(
            default_window_width, default_window_height, "glyph", NULL, NULL);
        // glfwSetWindowUserPointer
        glfwSetFramebufferSizeCallback(window, framebuffer_resize_callback);

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
} swapchain_support_details_t;

swapchain_support_details_t query_swapchain_support(VkPhysicalDevice device) {
        swapchain_support_details_t details = { 0 };

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

VkExtent2D choose_swap_extent(VkSurfaceCapabilitiesKHR cap) {
        if (cap.currentExtent.width != UINT32_MAX) {
                return cap.currentExtent;
        }
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actual = { width, height };

        if (actual.width < cap.minImageExtent.width) {
                actual.width = cap.minImageExtent.width;
        } else if (actual.width > cap.maxImageExtent.width) {
                actual.width = cap.maxImageExtent.width;
        }
        if (actual.height < cap.minImageExtent.height) {
                actual.height = cap.minImageExtent.height;
        } else if (actual.height > cap.maxImageExtent.height) {
                actual.height = cap.maxImageExtent.height;
        }

        return actual;
}

VkPresentModeKHR choose_sc_present_mode(swapchain_support_details_t details) {
        for (uint32_t i = 0; i < details.present_modes_count; i++) {
                if (details.present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                        return details.present_modes[i];
                }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
}

VkSurfaceFormatKHR
choose_sc_surface_format(swapchain_support_details_t details) {
        for (uint32_t i = 0; i < details.formats_count; i++) {
                VkSurfaceFormatKHR format = details.formats[i];
                if (format.format == VK_FORMAT_B8G8R8A8_SRGB
                    && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                        return format;
                }
        }
        return details.formats[0];
}

// Creates swapchain. Returns 1 on success, 0 on failure.
static int create_swapchain(void) {
        swapchain_support_details_t support
            = query_swapchain_support(physical_device);

        VkSurfaceFormatKHR surface_format = choose_sc_surface_format(support);
        VkPresentModeKHR present_mode = choose_sc_present_mode(support);
        VkExtent2D extent = choose_swap_extent(support.capabilities);

        uint32_t image_count = support.capabilities.minImageCount + 1;
        if (support.capabilities.maxImageCount > 0
            && image_count > support.capabilities.maxImageCount) {
                image_count = support.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createinfo = { 0 };
        createinfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createinfo.surface = surface;
        createinfo.minImageCount = image_count;
        createinfo.imageFormat = surface_format.format;
        createinfo.imageColorSpace = surface_format.colorSpace;
        createinfo.imageExtent = extent;
        createinfo.imageArrayLayers = 1;
        createinfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        queue_family_indicies_t indicies = find_queue_families(physical_device);
        uint32_t queue_family_indicies[]
            = { indicies.graphics.index, indicies.presentation.index };

        if (indicies.graphics.index != indicies.presentation.index) {
                createinfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
                createinfo.queueFamilyIndexCount = 2;
                createinfo.pQueueFamilyIndices = queue_family_indicies;
        } else {
                createinfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        createinfo.preTransform = support.capabilities.currentTransform;
        createinfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createinfo.presentMode = present_mode;
        createinfo.clipped = VK_TRUE;
        createinfo.oldSwapchain = VK_NULL_HANDLE;

        free(support.formats);
        free(support.present_modes);

        VkResult res
            = vkCreateSwapchainKHR(device, &createinfo, NULL, &swapchain);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create swapchain: %s\n",
                        string_VkResult(res));
                return 0;
        }

        vkGetSwapchainImagesKHR(device, swapchain, &swapchain_images_count,
                                NULL);
        swapchain_images = malloc(sizeof(VkImage) * swapchain_images_count);
        vkGetSwapchainImagesKHR(device, swapchain, &swapchain_images_count,
                                swapchain_images);
        swapchain_format = surface_format.format;
        swapchain_extent = extent;

        return 1;
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
                swapchain_support_details_t sc_support
                    = query_swapchain_support(device);
                swapchain_adequate = sc_support.formats_count
                                     && sc_support.present_modes_count;
                free(sc_support.formats);
                free(sc_support.present_modes);
        }

        return indicies.graphics.valid && indicies.presentation.valid
               && extensions_supported && swapchain_adequate;
}

// Creates image views. Returns 1 on success, 0 on failure.
static int create_image_views(void) {
        swapchain_image_views_count = swapchain_images_count;
        swapchain_image_views
            = malloc(sizeof(VkImageView) * swapchain_image_views_count);

        for (uint32_t i = 0; i < swapchain_image_views_count; i++) {
                VkImageViewCreateInfo createinfo = { 0 };
                createinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                createinfo.image = swapchain_images[i];
                createinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                createinfo.format = swapchain_format;
                createinfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
                createinfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                createinfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                createinfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
                createinfo.subresourceRange.aspectMask
                    = VK_IMAGE_ASPECT_COLOR_BIT;
                createinfo.subresourceRange.baseMipLevel = 0;
                createinfo.subresourceRange.levelCount = 1;
                createinfo.subresourceRange.baseArrayLayer = 0;
                createinfo.subresourceRange.layerCount = 1;

                VkResult res = vkCreateImageView(device, &createinfo, NULL,
                                                 &swapchain_image_views[i]);
                if (res != VK_SUCCESS) {
                        fprintf(stderr, "Failed to create image view: %s\n",
                                string_VkResult(res));
                        return 0;
                }
        }

        return 1;
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

        // The following is useful for checking the existence of an
        // extension uint32_t supported_ext_count = 0;
        // vkEnumerateInstanceExtensionProperties(NULL,
        // &supported_ext_count,
        //                                        NULL);
        // VkExtensionProperties *supported_exts
        //     = malloc(sizeof(VkExtensionProperties) *
        //     supported_ext_count);
        // vkEnumerateInstanceExtensionProperties(NULL,
        // &supported_ext_count,
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

// Create render pass. Returns 1 on success, 0 on failure.
static int create_render_pass(void) {
        VkAttachmentDescription color_attachment = {
                .format = swapchain_format,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        };

        VkAttachmentReference color_attach_ref = {
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };

        VkSubpassDescription subpass = {
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount = 1,
                .pColorAttachments = &color_attach_ref,
        };

        VkSubpassDependency dependency = {
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = 0,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        };

        VkRenderPassCreateInfo createinfo = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                .attachmentCount = 1,
                .pAttachments = &color_attachment,
                .subpassCount = 1,
                .pSubpasses = &subpass,
                .dependencyCount = 1,
                .pDependencies = &dependency,
        };

        VkResult res
            = vkCreateRenderPass(device, &createinfo, NULL, &render_pass);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create render pass: %s\n",
                        string_VkResult(res));
                return 0;
        }
        return 1;
}

// Create graphics pipeline. Returns 1 on success, 0 on failure.
static int create_graphics_pipeline(void) {
        VkShaderModule vert = create_shader_module(device, "shaders/vert.spv");
        VkShaderModule frag = create_shader_module(device, "shaders/frag.spv");
        // need to properly clean up shader modules
        if (vert == VK_NULL_HANDLE || frag == VK_NULL_HANDLE) {
                return 0;
        }

        VkPipelineShaderStageCreateInfo vert_stage_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vert,
                .pName = "main",
        };

        VkPipelineShaderStageCreateInfo frag_stage_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = frag,
                .pName = "main",
        };

        VkPipelineShaderStageCreateInfo shader_stages[] = {
                vert_stage_info,
                frag_stage_info,
        };

        VkDynamicState dynamic_states[] = {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR,
        };
        VkPipelineDynamicStateCreateInfo dynamic_state = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                .dynamicStateCount
                = sizeof(dynamic_states) / sizeof(dynamic_states[0]),
                .pDynamicStates = dynamic_states,
        };

        VkVertexInputBindingDescription vertex_binding_desc = {
                .binding = 0,
                .stride = sizeof(vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };

        VkVertexInputAttributeDescription vertex_attr_desc[2] = {
                {
                        .binding = 0,
                        .location = 0,
                        .format = VK_FORMAT_R32G32_SFLOAT,
                        .offset = offsetof(vertex, pos),
                },
                {
                        .binding = 0,
                        .location = 1,
                        .format = VK_FORMAT_R32G32B32_SFLOAT,
                        .offset = offsetof(vertex, color),
                },
        };

        VkPipelineVertexInputStateCreateInfo vertex_input_info = {
                .sType
                = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &vertex_binding_desc,
                .vertexAttributeDescriptionCount = 2,
                .pVertexAttributeDescriptions = vertex_attr_desc,
        };

        VkPipelineInputAssemblyStateCreateInfo input_assembly = {
                .sType
                = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                .primitiveRestartEnable = VK_FALSE,
        };

        VkPipelineViewportStateCreateInfo viewport_state = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .viewportCount = 1,
                .scissorCount = 1,
        };

        VkPipelineRasterizationStateCreateInfo rasterizer = {
                .sType
                = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .depthClampEnable = VK_FALSE,
                .rasterizerDiscardEnable = VK_FALSE,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .lineWidth = 1.0f,
                .cullMode = VK_CULL_MODE_BACK_BIT,
                .frontFace = VK_FRONT_FACE_CLOCKWISE,
                .depthBiasEnable = VK_FALSE,
        };

        VkPipelineMultisampleStateCreateInfo multisampling = {
                .sType
                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .sampleShadingEnable = VK_FALSE,
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        };

        VkPipelineColorBlendAttachmentState color_blend_attachment = {
                .colorWriteMask
                = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                  | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                .blendEnable = VK_FALSE,
        };

        VkPipelineColorBlendStateCreateInfo color_blending = {
                .sType
                = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .logicOpEnable = VK_FALSE,
                .logicOp = VK_LOGIC_OP_COPY,
                .attachmentCount = 1,
                .pAttachments = &color_blend_attachment,
        };

        VkPipelineLayoutCreateInfo pipeline_layout_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = 0,
                .pushConstantRangeCount = 0,
        };

        VkResult res = vkCreatePipelineLayout(device, &pipeline_layout_info,
                                              NULL, &pipeline_layout);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create pipeline layout: %s\n",
                        string_VkResult(res));
                return 0;
        }

        VkGraphicsPipelineCreateInfo pipeline_info = {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .stageCount = 2,
                .pStages = shader_stages,
                .pVertexInputState = &vertex_input_info,
                .pInputAssemblyState = &input_assembly,
                .pViewportState = &viewport_state,
                .pRasterizationState = &rasterizer,
                .pMultisampleState = &multisampling,
                .pDepthStencilState = NULL,
                .pColorBlendState = &color_blending,
                .pDynamicState = &dynamic_state,
                .layout = pipeline_layout,
                .renderPass = render_pass,
                .subpass = 0,
                .basePipelineHandle = VK_NULL_HANDLE,
                .basePipelineIndex = -1,
        };

        res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
                                        &pipeline_info, NULL,
                                        &graphics_pipeline);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create graphics pipelines: %s\n",
                        string_VkResult(res));
                return 0;
        }

        vkDestroyShaderModule(device, vert, NULL);
        vkDestroyShaderModule(device, frag, NULL);

        return 1;
}

// Create framebuffers. Returns 1 on success, 0 on failure.
static int create_framebuffers(void) {
        swapchain_framebuffer_count = swapchain_image_views_count;
        swapchain_framebuffers
            = malloc(sizeof(VkFramebuffer) * swapchain_framebuffer_count);

        for (uint32_t i = 0; i < swapchain_framebuffer_count; i++) {
                VkImageView attachments[] = {
                        swapchain_image_views[i],
                };

                VkFramebufferCreateInfo createinfo = { 0 };
                createinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                createinfo.renderPass = render_pass;
                createinfo.attachmentCount = 1;
                createinfo.pAttachments = attachments;
                createinfo.width = swapchain_extent.width;
                createinfo.height = swapchain_extent.height;
                createinfo.layers = 1;

                VkResult res = vkCreateFramebuffer(device, &createinfo, NULL,
                                                   swapchain_framebuffers + i);
                if (res != VK_SUCCESS) {
                        fprintf(stderr, "Failed to create framebuffer: %s\n",
                                string_VkResult(res));
                        return 0;
                }
        }

        return 1;
}

static void cleanup_swapchain(void) {
        for (uint32_t i = 0; i < swapchain_framebuffer_count; i++) {
                vkDestroyFramebuffer(device, swapchain_framebuffers[i], NULL);
        }
        for (uint32_t i = 0; i < swapchain_image_views_count; i++) {
                vkDestroyImageView(device, swapchain_image_views[i], NULL);
        }
        vkDestroySwapchainKHR(device, swapchain, NULL);
}

// Recreates swap chain. Returns 1 on success, 0 on failure.
static int recreate_swapchain(void) {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while (width == 0 || height == 0) {
                glfwGetFramebufferSize(window, &width, &height);
                glfwWaitEvents();
        }

        vkDeviceWaitIdle(device);

        cleanup_swapchain();

        int res = create_swapchain();
        if (!res)
                return 0;

        res = create_image_views();
        if (!res)
                return 0;

        res = create_framebuffers();
        if (!res)
                return 0;

        return 1;
}

// Creates a command pool. Returns 1 on success, 0 on failure.
static int create_command_pool(void) {
        queue_family_indicies_t indicies = find_queue_families(physical_device);

        VkCommandPoolCreateInfo createinfo = { 0 };
        createinfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        createinfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        createinfo.queueFamilyIndex = indicies.graphics.index;

        VkResult res
            = vkCreateCommandPool(device, &createinfo, NULL, &command_pool);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create command pool: %s\n",
                        string_VkResult(res));
                return 0;
        }

        return 1;
}

// Create command buffer. Return 1 on success, 0 on failure.
static int create_command_buffer(void) {
        VkCommandBufferAllocateInfo allocinfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = command_pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
        };

        VkResult res
            = vkAllocateCommandBuffers(device, &allocinfo, command_buffer);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to allocate command buffers: %s\n",
                        string_VkResult(res));
                return 0;
        }

        return 1;
}

// Write commands into the command buffer. Returns 1 on success, 0 on failure
static int record_command_buffer(VkCommandBuffer cmd_buffer,
                                 uint32_t image_index) {
        VkCommandBufferBeginInfo begininfo = { 0 };
        begininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begininfo.flags = 0;
        begininfo.pInheritanceInfo = NULL;

        VkResult res = vkBeginCommandBuffer(cmd_buffer, &begininfo);
        if (res != VK_SUCCESS) {
                fprintf(stderr,
                        "Failed to being command buffer recording: %s\n",
                        string_VkResult(res));
                return 0;
        }

        VkClearValue clear_value = { { { 0.3f, 0.3f, 0.3f, 1.0f } } };
        VkRenderPassBeginInfo passinfo = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .renderPass = render_pass,
                .framebuffer = swapchain_framebuffers[image_index],
                .renderArea.offset = { 0, 0 },
                .renderArea.extent = swapchain_extent,
                .clearValueCount = 1,
                .pClearValues = &clear_value,
        };

        vkCmdBeginRenderPass(cmd_buffer, &passinfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          graphics_pipeline);

        VkViewport viewport = {
                .x = 0.0f,
                .y = 0.0f,
                .width = (float)swapchain_extent.width,
                .height = (float)swapchain_extent.height,
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
        };
        vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);

        VkRect2D scissor = {
                .offset = { 0, 0 },
                .extent = swapchain_extent,
        };
        vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

        vkCmdDraw(cmd_buffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd_buffer);

        res = vkEndCommandBuffer(cmd_buffer);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to record command buffer: %s\n",
                        string_VkResult(res));
                return 0;
        }

        return 1;
}

// Create semaphores and fences. Returns 1 on success, 0 on failure.
static int create_sync_objects(void) {
        VkSemaphoreCreateInfo semaphore_info = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };
        VkFenceCreateInfo fence_info = {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                VkResult res = vkCreateSemaphore(device, &semaphore_info, NULL,
                                                 image_available_semaphore + i);
                if (res != VK_SUCCESS) {
                        fprintf(
                            stderr,
                            "Failed to create image available semaphore: %s\n",
                            string_VkResult(res));
                        return 0;
                }
                res = vkCreateSemaphore(device, &semaphore_info, NULL,
                                        render_finished_semaphore + i);
                if (res != VK_SUCCESS) {
                        fprintf(
                            stderr,
                            "Failed to create render finished semaphore: %s\n",
                            string_VkResult(res));
                        return 0;
                }
                res = vkCreateFence(device, &fence_info, NULL,
                                    inflight_fence + i);
                if (res != VK_SUCCESS) {
                        fprintf(stderr,
                                "Failed to create in flight fence: %s\n",
                                string_VkResult(res));
                        return 0;
                }
        }

        return 1;
}

// Submit a frame to be drawn. Returns 1 on success, 0 on failure.
static int draw_frame(void) {
        vkWaitForFences(device, 1, inflight_fence + current_frame, VK_TRUE,
                        UINT64_MAX);

        uint32_t image_index;
        VkResult res
            = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                    image_available_semaphore[current_frame],
                                    VK_NULL_HANDLE, &image_index);
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR
            || framebuffer_resized) {
                framebuffer_resized = 0;
                recreate_swapchain();
        } else if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to acquire swap chain image: %s\n",
                        string_VkResult(res));
                return 0;
        }

        vkResetFences(device, 1, inflight_fence + current_frame);

        vkResetCommandBuffer(command_buffer[current_frame], 0);
        record_command_buffer(command_buffer[current_frame], image_index);

        VkPipelineStageFlags wait_stages[] = {
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        };
        VkSubmitInfo submit_info = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = image_available_semaphore + current_frame,
                .pWaitDstStageMask = wait_stages,
                .commandBufferCount = 1,
                .pCommandBuffers = command_buffer + current_frame,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = render_finished_semaphore + current_frame,
        };

        res = vkQueueSubmit(graphics_queue, 1, &submit_info,
                            inflight_fence[current_frame]);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to sumbit draw command buffer: %s\n",
                        string_VkResult(res));
                return 0;
        }

        VkSwapchainKHR swapchains[] = { swapchain };
        VkPresentInfoKHR present_info = {
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = render_finished_semaphore + current_frame,
                .swapchainCount = 1,
                .pSwapchains = swapchains,
                .pImageIndices = &image_index,
                .pResults = NULL,
        };

        res = vkQueuePresentKHR(presentation_queue, &present_info);
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
                recreate_swapchain();
        } else if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to present swap chain image: %s",
                        string_VkResult(res));
                return 0;
        }

        current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

        return 1;
}

// Create vertex buffers. Returns 1 on success, 0 on failure.
static int create_vertex_buffer(void) {
        VkBufferCreateInfo create_info = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = sizeof(verticies[0]),
        };

        return 1;
}

int main(int argc, char **argv) {
        window = init_window();

        if (!create_instance()) {
                return 1;
        }
        if (enable_validation_layers && !setup_debug_messenger(instance)) {
                return 1;
        }
        if (!create_surface()) {
                return 1;
        }
        if (!pick_physical_device()) {
                return 1;
        }
        if (!create_logical_device()) {
                return 1;
        }
        if (!create_swapchain()) {
                return 1;
        }
        if (!create_image_views()) {
                return 1;
        }
        if (!create_render_pass()) {
                return 1;
        }
        if (!create_graphics_pipeline()) {
                return 1;
        }
        if (!create_framebuffers()) {
                return 1;
        }
        if (!create_command_pool()) {
                return 1;
        }
        if (!create_vertex_buffer()) {
                return 1;
        }
        if (!create_command_buffer()) {
                return 1;
        }
        if (!create_sync_objects()) {
                return 1;
        }

        while (!glfwWindowShouldClose(window)) {
                glfwPollEvents();
                draw_frame();
        }
        vkDeviceWaitIdle(device);

        if (enable_validation_layers) {
                destroy_debug_utils_messenger_ext(instance, debug_messenger,
                                                  NULL);
        }
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                vkDestroySemaphore(device, image_available_semaphore[i], NULL);
                vkDestroySemaphore(device, render_finished_semaphore[i], NULL);
                vkDestroyFence(device, inflight_fence[i], NULL);
        }
        vkDestroyCommandPool(device, command_pool, NULL);
        cleanup_swapchain();
        vkDestroyPipeline(device, graphics_pipeline, NULL);
        vkDestroyPipelineLayout(device, pipeline_layout, NULL);
        vkDestroyRenderPass(device, render_pass, NULL);
        vkDestroyDevice(device, NULL);
        vkDestroySurfaceKHR(instance, surface, NULL);
        vkDestroyInstance(instance, NULL);
        glfwDestroyWindow(window);

        glfwTerminate();
        return 0;
}
