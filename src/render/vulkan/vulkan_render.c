/* Vulkan validation layer and debug extension */
#ifdef NDEBUG
static u32 enable_validation_layers = 0;
#else
static u32 enable_validation_layers = 1;
#endif

static const char *validation_layers[] = {
        "VK_LAYER_KHRONOS_validation",
};
u32 validation_layer_count
    = sizeof(validation_layers) / sizeof(validation_layers[0]);

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

static RState r_state = { .width = 1000, .height = 700 };

static void r_assert(b32 flag, char *msg) {
        if (!flag) {
                printf("ASSERT: %s\n", msg);
                u32 *bomb = 0;
                *bomb = 1;
        }
}

static void r_check_vkresult(VkResult res, char *msg) {
        if (res != VK_SUCCESS) {
                printf("ASSERT: %s %s\n", msg, string_VkResult(res));
                u32 *bomb = 0;
                *bomb = 1;
        }
}

static void framebuffer_resize_callback(GLFWwindow *window, int width,
                                        int height) {}

static const char **r_get_required_extensions(Arena *a, u32 *extCount) {
        u32 glfwExtCount = 0;
        const char **glfwExts
            = glfwGetRequiredInstanceExtensions(&glfwExtCount);

        u32 count
            = enable_validation_layers ? glfwExtCount + 3 : glfwExtCount + 2;
        const char **extNames = arena_alloc(a, sizeof(char *) * count);

        for (int i = 0; i < glfwExtCount; i++) {
                extNames[i] = glfwExts[i];
        }

        if (enable_validation_layers) {
                extNames[count - 3] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        }
        // hr: macOs extensions
        extNames[count - 2] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
        extNames[count - 1]
            = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;

        *extCount = count;
        return extNames;
}

static b32 r_check_validation_layer_support(Arena *a) {
        u32 layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, 0);
        r_assert(layerCount != 0, "Failed to find any vulkan layers");

        VkLayerProperties *layersAvailable
            = arena_alloc(a, sizeof(VkLayerProperties *) * layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, layersAvailable);

        for (u32 i = 0; i < validation_layer_count; i++) {
                b32 layerFound = 0;
                const char *layer = validation_layers[i];
                for (int j = 0; j < layerCount; j++) {
                        if (strcmp(layer, layersAvailable[j].layerName) == 0) {
                                layerFound = 1;
                                break;
                        }
                }

                if (!layerFound) {
                        return 0;
                }
        }

        return 1;
}

static const char *device_exts[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        "VK_KHR_portability_subset", // hr: macOs device extensions
};
u32 device_ext_count = sizeof(device_exts) / sizeof(device_exts[0]);

static b32 r_device_supports_extensions(Arena *a, VkPhysicalDevice device) {
        u32 availableCount = 0;
        vkEnumerateDeviceExtensionProperties(device, 0, &availableCount, 0);

        VkExtensionProperties *available
            = arena_alloc(a, sizeof(VkExtensionProperties) * availableCount);
        vkEnumerateDeviceExtensionProperties(device, 0, &availableCount,
                                             available);

        for (u32 i = 0; i < device_ext_count; i++) {
                const char *ext = device_exts[i];
                b32 found = 0;
                for (u32 j = 0; j < availableCount; j++) {
                        if (strcmp(ext, available[j].extensionName) == 0) {
                                found = 1;
                                break;
                        }
                }

                if (!found) {
                        return found;
                }
        }

        return 1;
}

static void r_pick_physical_device(Arena *a) {
        u32 deviceCount = 0;
        vkEnumeratePhysicalDevices(r_state.instance, &deviceCount, 0);
        r_assert(deviceCount != 0, "No physical devices found");

        VkPhysicalDevice *devices
            = arena_alloc(a, sizeof(VkPhysicalDevice) * deviceCount);
        vkEnumeratePhysicalDevices(r_state.instance, &deviceCount, devices);

        for (u32 i = 0; i < deviceCount; i++) {
                VkPhysicalDevice device = devices[i];

                VkPhysicalDeviceProperties props;
                vkGetPhysicalDeviceProperties(device, &props);

                b32 supportsExtensions
                    = r_device_supports_extensions(a, device);

                VkSurfaceCapabilitiesKHR capabilities;
                u32 formatsCount;
                VkSurfaceFormatKHR *formats;
                u32 presentModesCount;
                VkPresentModeKHR *presentModes;
                vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                    device, r_state.surface, &capabilities);

                vkGetPhysicalDeviceSurfaceFormatsKHR(device, r_state.surface,
                                                     &formatsCount, 0);
                formats
                    = arena_alloc(a, sizeof(VkSurfaceFormatKHR) * formatsCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(device, r_state.surface,
                                                     &formatsCount, formats);

                vkGetPhysicalDeviceSurfacePresentModesKHR(
                    device, r_state.surface, &presentModesCount, 0);
                presentModes = arena_alloc(a, sizeof(VkPresentModeKHR)
                                                  * presentModesCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(
                    device, r_state.surface, &presentModesCount, presentModes);

                b32 swapchainAdequate = formatsCount && presentModesCount;

                u32 queueFamilyCount = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(device,
                                                         &queueFamilyCount, 0);
                VkQueueFamilyProperties *queueFamilyProperties = arena_alloc(
                    a, sizeof(VkQueueFamilyProperties) * queueFamilyCount);
                vkGetPhysicalDeviceQueueFamilyProperties(
                    device, &queueFamilyCount, queueFamilyProperties);
                for (u32 j = 0; j < queueFamilyCount; j++) {
                        b32 supportsPresent = 0;
                        vkGetPhysicalDeviceSurfaceSupportKHR(
                            device, j, r_state.surface, &supportsPresent);

                        b32 graphicsQueue = queueFamilyProperties[j].queueFlags
                                            & VK_QUEUE_GRAPHICS_BIT;

                        if (graphicsQueue && supportsPresent
                            && supportsExtensions && swapchainAdequate) {
                                r_state.physicalDevice = device;
                                r_state.physicalDeviceProps = props;
                                r_state.graphicsQueueIdx = j;
                                r_state.presentQueueIdx = j;

                                r_state.colorFormat = formats[0].format;
                                if (formatsCount == 0
                                    && formats[0].format
                                           == VK_FORMAT_UNDEFINED) {
                                        r_state.colorFormat
                                            = VK_FORMAT_B8G8R8_UNORM;
                                }
                                r_state.colorSpace = formats[0].colorSpace;

                                r_state.imageCount = 2;
                                if (r_state.imageCount
                                    < capabilities.minImageCount) {
                                        r_state.imageCount
                                            = capabilities.minImageCount;
                                } else if (capabilities.maxImageCount != 0
                                           && r_state.imageCount
                                                  > capabilities
                                                        .maxImageCount) {
                                        r_state.imageCount
                                            = capabilities.maxImageCount;
                                }

                                r_state.resolution = capabilities.currentExtent;
                                if (r_state.resolution.width == -1) {
                                        r_state.resolution.width
                                            = r_state.width;
                                        r_state.resolution.height
                                            = r_state.height;
                                } else {
                                        r_state.width
                                            = r_state.resolution.width;
                                        r_state.height
                                            = r_state.resolution.height;
                                }

                                r_state.preTransform
                                    = capabilities.currentTransform;

                                r_state.presentMode = VK_PRESENT_MODE_FIFO_KHR;
                                for (u32 k = 0; k < presentModesCount; k++) {
                                        if (presentModes[k]
                                            == VK_PRESENT_MODE_MAILBOX_KHR) {
                                                r_state.presentMode
                                                    = presentModes[k];
                                                break;
                                        }
                                }
                                return;
                        }
                }
        }

        r_assert(0, "No suitable physical devices found");
}

s64 r_find_memory_type(u32 typeFilter, VkMemoryPropertyFlags props) {
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(r_state.physicalDevice, &memProps);

        for (u32 i = 0; i < memProps.memoryTypeCount; i++) {
                if (typeFilter & (1 << i)
                    && (memProps.memoryTypes[i].propertyFlags & props)
                           == props) {
                        return i;
                }
        }

        return -1;
}

static void r_create_buffer(VkBuffer *buffer, VkDeviceMemory *memory,
                            VkDeviceSize size, VkBufferUsageFlags usage,
                            VkMemoryPropertyFlags props) {
        VkBufferCreateInfo bufferCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = size,
                .usage = usage,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };

        VkResult res
            = vkCreateBuffer(r_state.device, &bufferCreateInfo, 0, buffer);
        r_check_vkresult(res, "Failed to create buffer");

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(r_state.device, *buffer,
                                      &memRequirements);

        s64 memType = r_find_memory_type(memRequirements.memoryTypeBits, props);
        r_assert(memType >= 0, "Failed to find suitable memory type");

        VkMemoryAllocateInfo allocInfo = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = memRequirements.size,
                .memoryTypeIndex = memType,
        };
        res = vkAllocateMemory(r_state.device, &allocInfo, 0, memory);
        r_check_vkresult(res, "Failed to allocate buffer memory");

        vkBindBufferMemory(r_state.device, *buffer, *memory, 0);
}

static void r_copy_buffer(VkCommandBuffer cmdBuffer, VkBuffer src, VkBuffer dst,
                          VkDeviceSize size) {
        VkCommandBufferBeginInfo beginInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(cmdBuffer, &beginInfo);

        VkBufferCopy copyRegion = {
                .srcOffset = 0,
                .dstOffset = 0,
                .size = size,
        };
        vkCmdCopyBuffer(cmdBuffer, src, dst, 1, &copyRegion);

        vkEndCommandBuffer(cmdBuffer);

        VkSubmitInfo submitInfo = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &cmdBuffer,
        };
        vkQueueSubmit(r_state.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);

        vkQueueWaitIdle(r_state.graphicsQueue);

        vkResetCommandBuffer(cmdBuffer, 0);
}

// hr: the whole shader section needs to be updated to be os independent
#include <sys/stat.h>

static String8 r_read_shader_file(const char *filename, String8 buf) {
        FILE *fp = fopen(filename, "rb");
        r_assert(fp != NULL, "Failed to open shader file");

        size_t res = fread(buf.data, 1, buf.length, fp);
        fclose(fp);
        r_assert(res > 0, "Failed to read in shader file");

        return buf;
}

static String8 r_alloc_shader_buffer(Arena *a, const char *filename) {
        String8 res;

        struct stat s;
        stat(filename, &s);

        res = string8_allocate(a, s.st_size);
        return res;
}

static VkShaderModule r_create_shader_module(const char *filename) {
        VkShaderModule shader = VK_NULL_HANDLE;

        String8 code = r_alloc_shader_buffer(&r_state.arena, filename);
        code = r_read_shader_file(filename, code);
        VkShaderModuleCreateInfo createInfo = {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .codeSize = code.length,
                .pCode = (u32 *)code.data,
        };

        VkResult res
            = vkCreateShaderModule(r_state.device, &createInfo, 0, &shader);
        r_check_vkresult(res, "Failed to create shader module");

        return shader;
}

// hr: TODO - rework this function into new assert style
static VkPipeline
r_create_graphics_pipeline(RGraphicsPipelineCreateInfo *createInfo) {
        VkDevice device = r_state.device;

        if (!createInfo->vertFile || !createInfo->fragFile) {
                fprintf(stderr, "Shader filename is null pointer.\n");
                return 0;
        }
        VkShaderModule vert = r_create_shader_module(createInfo->vertFile);
        VkShaderModule frag = r_create_shader_module(createInfo->fragFile);
        // need to properly clean up shader modules
        if (vert == VK_NULL_HANDLE || frag == VK_NULL_HANDLE) {
                return VK_NULL_HANDLE;
        }

        VkPipelineShaderStageCreateInfo vertStageInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vert,
                .pName = "main",
        };

        VkPipelineShaderStageCreateInfo fragStageInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = frag,
                .pName = "main",
        };

        VkPipelineShaderStageCreateInfo shaderStages[] = {
                vertStageInfo,
                fragStageInfo,
        };

        VkDynamicState dynamicStates[] = {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR,
        };
        VkPipelineDynamicStateCreateInfo dynamicState = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                .dynamicStateCount
                = sizeof(dynamicStates) / sizeof(dynamicStates[0]),
                .pDynamicStates = dynamicStates,
        };

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
                .sType
                = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .topology = createInfo->primativeTopology,
                .primitiveRestartEnable = VK_FALSE,
        };

        VkPipelineViewportStateCreateInfo viewportState = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .viewportCount = 1,
                .scissorCount = 1,
        };

        VkPipelineRasterizationStateCreateInfo rasterizer = {
                .sType
                = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .polygonMode = createInfo->polygonMode,
                .lineWidth = 1.0f,
                .cullMode = createInfo->cullMode,
                .frontFace = createInfo->frontFace,
        };

        VkPipelineMultisampleStateCreateInfo multisampling = {
                .sType
                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        };

        VkPipelineColorBlendStateCreateInfo colorBlending = {
                .sType
                = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .attachmentCount = createInfo->blendAttachmentStatesCount,
                .pAttachments = createInfo->blendAttachmentStates,
        };

        VkGraphicsPipelineCreateInfo pipelineInfo = {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .stageCount = 2,
                .pStages = shaderStages,
                .pVertexInputState = createInfo->vertexInputInfo,
                .pInputAssemblyState = &inputAssembly,
                .pViewportState = &viewportState,
                .pRasterizationState = &rasterizer,
                .pMultisampleState = &multisampling,
                .pDepthStencilState = createInfo->depthStencilState,
                .pColorBlendState = &colorBlending,
                .pDynamicState = &dynamicState,
                .layout = createInfo->pipelineLayout,
                .renderPass = createInfo->renderPass,
                .basePipelineIndex = -1,
        };

        VkPipeline pipeline = VK_NULL_HANDLE;
        VkResult res = vkCreateGraphicsPipelines(
            device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &pipeline);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create graphics pipelines: %s\n",
                        string_VkResult(res));
                return VK_NULL_HANDLE;
        }

        vkDestroyShaderModule(device, vert, NULL);
        vkDestroyShaderModule(device, frag, NULL);

        return pipeline;
}

static void r_render_init(void) {
        r_state.arena = make_arena(0xFF00);

        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        String8 windowName = string8_lit("vulkan start");
        r_state.window = glfwCreateWindow(r_state.width, r_state.height,
                                          (const char *)windowName.data, 0, 0);
        glfwSetFramebufferSizeCallback(r_state.window,
                                       framebuffer_resize_callback);

        // hr: Load vulkan functions from dynamic library on system
        VkApplicationInfo appInfo = {
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pApplicationName = "grove",
                .engineVersion = 1,
                .apiVersion = VK_MAKE_VERSION(1, 0, 0),
        };

        u32 extCount = 0;
        const char **extNames
            = r_get_required_extensions(&r_state.arena, &extCount);
        VkInstanceCreateInfo instanceInfo = {
                .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                .pApplicationInfo = &appInfo,
                .enabledExtensionCount = extCount,
                .ppEnabledExtensionNames = extNames,
                .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
        };

        if (enable_validation_layers) {
                r_assert(r_check_validation_layer_support(&r_state.arena),
                         "Failed to find validation layers");
                instanceInfo.enabledLayerCount = validation_layer_count;
                instanceInfo.ppEnabledLayerNames = validation_layers;

                VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = { 0 };
                populate_debug_messenger_createinfo(&debugCreateInfo);
                instanceInfo.pNext = &debugCreateInfo;
        }

        VkResult res = vkCreateInstance(&instanceInfo, 0, &r_state.instance);
        r_check_vkresult(res, "Failed to create vulkan instance");

        if (enable_validation_layers) {
                r_assert(setup_debug_messenger(r_state.instance), "");
        }

        res = glfwCreateWindowSurface(r_state.instance, r_state.window, 0,
                                      &r_state.surface);
        r_check_vkresult(res, "Failed to create surface");

        r_pick_physical_device(&r_state.arena);

        float queuePriorities[] = { 1.0 };
        VkDeviceQueueCreateInfo queueCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = r_state.presentQueueIdx,
                .queueCount = 1,
                .pQueuePriorities = queuePriorities,
        };

        VkPhysicalDeviceFeatures deviceFeatures = { 0 };

        VkDeviceCreateInfo deviceCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .queueCreateInfoCount = 1,
                .pQueueCreateInfos = &queueCreateInfo,
                .enabledLayerCount = validation_layer_count,
                .ppEnabledLayerNames = validation_layers,
                .enabledExtensionCount = device_ext_count,
                .ppEnabledExtensionNames = device_exts,
                .pEnabledFeatures = &deviceFeatures,
        };
        res = vkCreateDevice(r_state.physicalDevice, &deviceCreateInfo, 0,
                             &r_state.device);
        r_check_vkresult(res, "Failed to create logical device");

        vkGetDeviceQueue(r_state.device, r_state.graphicsQueueIdx, 0,
                         &r_state.graphicsQueue);
        vkGetDeviceQueue(r_state.device, r_state.presentQueueIdx, 0,
                         &r_state.presentQueue);

        VkSwapchainCreateInfoKHR swapchainCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                .surface = r_state.surface,
                .minImageCount = r_state.imageCount,
                .imageFormat = r_state.colorFormat,
                .imageColorSpace = r_state.colorSpace,
                .imageExtent = r_state.resolution,
                .imageArrayLayers = 1,
                .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                .imageSharingMode
                = VK_SHARING_MODE_EXCLUSIVE, // hr: if graphics and present
                                             // queues are different then we'll
                                             // have to use concurrent sharing
                                             // mode.
                .preTransform = r_state.preTransform,
                .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                .presentMode = r_state.presentMode,
                .clipped = VK_TRUE,
        };
        res = vkCreateSwapchainKHR(r_state.device, &swapchainCreateInfo, 0,
                                   &r_state.swapchain);
        r_check_vkresult(res, "Failed to create swapchain");

        vkGetSwapchainImagesKHR(r_state.device, r_state.swapchain,
                                &r_state.imageCount, 0);
        r_state.swapchainImages
            = arena_alloc(&r_state.arena, sizeof(VkImage) * r_state.imageCount);
        vkGetSwapchainImagesKHR(r_state.device, r_state.swapchain,
                                &r_state.imageCount, r_state.swapchainImages);

        r_state.maxFramesInFlight = r_state.imageCount;

        r_state.swapchainImageViews = arena_alloc(
            &r_state.arena, sizeof(VkImageView) * r_state.imageCount);
        for (u32 i = 0; i < r_state.imageCount; i++) {
                VkImageViewCreateInfo imageViewCreateInfo = {
                        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                        .image = r_state.swapchainImages[i],
                        .viewType = VK_IMAGE_VIEW_TYPE_2D,
                        .format = r_state.colorFormat,
                        .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .subresourceRange.aspectMask
                        = VK_IMAGE_ASPECT_COLOR_BIT,
                        .subresourceRange.baseMipLevel = 0,
                        .subresourceRange.levelCount = 1,
                        .subresourceRange.baseArrayLayer = 0,
                        .subresourceRange.layerCount = 1,
                };
                res = vkCreateImageView(r_state.device, &imageViewCreateInfo, 0,
                                        &r_state.swapchainImageViews[i]);
                r_check_vkresult(res, "Failed to create swapchain image view");
        }

        VkAttachmentDescription passAttachments[] = {
                {
                        .format = r_state.colorFormat,
                        .samples = VK_SAMPLE_COUNT_1_BIT,
                        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                },

        };

        VkAttachmentReference colorAttachmentReference = {
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };

        VkSubpassDescription subpass = {
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount = 1,
                .pColorAttachments = &colorAttachmentReference,
        };

        VkSubpassDependency dependency = {
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = 0,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        };

        u32 attachmentCount
            = sizeof(passAttachments) / sizeof(passAttachments[0]);
        VkRenderPassCreateInfo renderPassCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                .attachmentCount = attachmentCount,
                .pAttachments = passAttachments,
                .subpassCount = 1,
                .pSubpasses = &subpass,
                .dependencyCount = 1,
                .pDependencies = &dependency,
        };

        res = vkCreateRenderPass(r_state.device, &renderPassCreateInfo, 0,
                                 &r_state.renderPass);
        r_check_vkresult(res, "Failed to create render pass");

        r_state.swapchainFramebuffers = arena_alloc(
            &r_state.arena, sizeof(VkFramebuffer) * r_state.imageCount);

        for (uint32_t i = 0; i < r_state.imageCount; i++) {
                VkImageView attachments[] = {
                        r_state.swapchainImageViews[i],
                };

                VkFramebufferCreateInfo framebufferCreateInfo = {
                        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                        .renderPass = r_state.renderPass,
                        .attachmentCount = attachmentCount,
                        .pAttachments = attachments,
                        .width = r_state.width,
                        .height = r_state.height,
                        .layers = 1,
                };

                res = vkCreateFramebuffer(r_state.device,
                                          &framebufferCreateInfo, 0,
                                          &r_state.swapchainFramebuffers[i]);
        }

        VkCommandPoolCreateInfo commandPoolCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = r_state.presentQueueIdx,
        };
        res = vkCreateCommandPool(r_state.device, &commandPoolCreateInfo, 0,
                                  &r_state.commandPool);
        r_check_vkresult(res, "Failed to create command pool");

        VkCommandBufferAllocateInfo commandBufferAllocationInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = r_state.commandPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = r_state.maxFramesInFlight,
        };
        r_state.renderCmdBuffers
            = arena_alloc(&r_state.arena,
                          sizeof(VkCommandBuffer) * r_state.maxFramesInFlight);
        res = vkAllocateCommandBuffers(r_state.device,
                                       &commandBufferAllocationInfo,
                                       r_state.renderCmdBuffers);
        r_check_vkresult(res, "Failed to allocate render command buffer");

        commandBufferAllocationInfo.commandBufferCount = 1;
        res = vkAllocateCommandBuffers(r_state.device,
                                       &commandBufferAllocationInfo,
                                       &r_state.setupCmdBuffer);
        r_check_vkresult(res, "Failed to allocate setup command buffer");
        //
        // VkBuffer stagingBuffer;
        // VkDeviceMemory stagingMemory;
        // VkDeviceSize verticesSize = sizeof(r_vertices);
        //
        // r_create_buffer(&stagingBuffer, &stagingMemory, verticesSize,
        //                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        //                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        //                     | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        //
        // void *vertexStagingData;
        // vkMapMemory(r_state.device, stagingMemory, 0, verticesSize, 0,
        //             &vertexStagingData);
        // memcpy(vertexStagingData, r_vertices, verticesSize);
        // vkUnmapMemory(r_state.device, stagingMemory);
        //
        // r_create_buffer(&r_state.vertexBuffer, &r_state.vertexMemory,
        //                 verticesSize,
        //                 VK_BUFFER_USAGE_TRANSFER_DST_BIT
        //                     | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        //                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        //
        // r_copy_buffer(r_state.setupCmdBuffer, stagingBuffer,
        //               r_state.vertexBuffer, verticesSize);
        //
        // vkDestroyBuffer(r_state.device, stagingBuffer, 0);
        // vkFreeMemory(r_state.device, stagingMemory, 0);
        //
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = 0,
                .pSetLayouts = 0, // hr: Not setting any bindings!!
                .pushConstantRangeCount = 0,
                .pPushConstantRanges = 0,
        };
        res = vkCreatePipelineLayout(r_state.device, &pipelineLayoutCreateInfo,
                                     0, &r_state.pipelineLayout);
        r_check_vkresult(res, "Failed to create pipeline layout");

        // VkVertexInputBindingDescription vertexBindingDescription = {
        //         .binding = 0,
        //         .stride = sizeof(Vec3),
        //         .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        // };
        //
        // VkVertexInputAttributeDescription vertexAttributeDescription = {
        //         .location = 0,
        //         .binding = 0,
        //         .format = VK_FORMAT_R32G32B32_SFLOAT,
        //         .offset = 0,
        // };
        //
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
                .sType
                = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                // .vertexBindingDescriptionCount = 1,
                // .pVertexBindingDescriptions = &vertexBindingDescription,
                // .vertexAttributeDescriptionCount = 1,
                // .pVertexAttributeDescriptions = &vertexAttributeDescription,
        };

        VkPipelineColorBlendAttachmentState colorBlendAttachment = {
                .colorWriteMask
                = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                  | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                .blendEnable = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
        };

        RGraphicsPipelineCreateInfo pipelineInfo = {
                .renderPass = r_state.renderPass,
                .pipelineLayout = r_state.pipelineLayout,
                .vertFile = "src/render/vulkan/vert.spv",
                .fragFile = "src/render/vulkan/frag.spv",
                .vertexInputInfo = &vertexInputInfo,
                .blendAttachmentStatesCount = 1,
                .blendAttachmentStates = &colorBlendAttachment,
                .depthStencilState = NULL,
                .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .cullMode = VK_CULL_MODE_BACK_BIT,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .primativeTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        };
        r_state.pipeline = r_create_graphics_pipeline(&pipelineInfo);

        VkSemaphoreCreateInfo semaphoreInfo = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };
        VkFenceCreateInfo fenceInfo = {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };

        r_state.imageAvailableSemaphore = arena_alloc(
            &r_state.arena, sizeof(VkSemaphore) * r_state.maxFramesInFlight);
        r_state.renderFinishedSemaphore = arena_alloc(
            &r_state.arena, sizeof(VkSemaphore) * r_state.maxFramesInFlight);
        r_state.inflightFence = arena_alloc(
            &r_state.arena, sizeof(VkFence) * r_state.maxFramesInFlight);

        for (u32 i = 0; i < r_state.maxFramesInFlight; i++) {
                res = vkCreateSemaphore(r_state.device, &semaphoreInfo, 0,
                                        r_state.imageAvailableSemaphore + i);
                r_check_vkresult(res,
                                 "Failed to create image available semaphore");
                res = vkCreateSemaphore(r_state.device, &semaphoreInfo, 0,
                                        r_state.renderFinishedSemaphore + i);
                r_check_vkresult(res,
                                 "Failed to create render finished semaphore");
                res = vkCreateFence(r_state.device, &fenceInfo, 0,
                                    r_state.inflightFence + i);
                r_check_vkresult(res, "Failed to create in flight fence");
        }

        RUniformBuffer ub = { 0 };
        r_create_buffer(ub.buffer, ub.memory, sizeof(ub),
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                            | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        void *ubMapped;
        vkMapMemory(r_state.device, r_state.memory, 0, sizeof(ub), 0, ubMapped);

        // Loop
        while (!glfwWindowShouldClose(r_state.window)) {

                u32 currentFrame = r_state.currentFrame;
                VkDevice device = r_state.device;
                vkWaitForFences(device, 1, r_state.inflightFence + currentFrame,
                                VK_TRUE, UINT64_MAX);

                u32 imageIdx;
                res = vkAcquireNextImageKHR(
                    r_state.device, r_state.swapchain, UINT64_MAX,
                    r_state.imageAvailableSemaphore[currentFrame],
                    VK_NULL_HANDLE, &imageIdx);
                if (res == VK_ERROR_OUT_OF_DATE_KHR
                    || res == VK_SUBOPTIMAL_KHR) {
                        r_assert(0, "TODO: handle framebuffer resized");
                }

                vkResetFences(device, 1, r_state.inflightFence + currentFrame);

                VkCommandBuffer cmdBuffer
                    = r_state.renderCmdBuffers[currentFrame];
                vkResetCommandBuffer(cmdBuffer, 0);

                VkCommandBufferBeginInfo beginInfo = {
                        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                };

                res = vkBeginCommandBuffer(cmdBuffer, &beginInfo);
                r_check_vkresult(res,
                                 "Failed to being command buffer recording");

                VkClearValue clear_value = { { { 0.3f, 0.3f, 0.3f, 1.0f } } };
                VkRenderPassBeginInfo passInfo = {
                        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                        .renderPass = r_state.renderPass,
                        .framebuffer = r_state.swapchainFramebuffers[imageIdx],
                        .renderArea.offset = { 0, 0 },
                        .renderArea.extent = { r_state.width, r_state.height },
                        .clearValueCount = 1,
                        .pClearValues = &clear_value,
                };

                vkCmdBeginRenderPass(cmdBuffer, &passInfo,
                                     VK_SUBPASS_CONTENTS_INLINE);

                vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  r_state.pipeline);

                VkViewport viewport = {
                        .x = 0.0f,
                        .y = 0.0f,
                        .width = (float)r_state.width,
                        .height = (float)r_state.height,
                        .minDepth = 0.0f,
                        .maxDepth = 1.0f,
                };
                vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

                VkRect2D scissor = {
                        .offset = { 0, 0 },
                        .extent = { r_state.width, r_state.height },
                };
                vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

                // VkDeviceSize vertexOffsets = {};
                // vkCmdBindVertexBuffers(cmdBuffer, 0, 1,
                // &r_state.vertexBuffer,
                //                        &vertexOffsets);
                // vkCmdBindDescriptorSets(cmdBuffer,
                //                         VK_PIPELINE_BIND_POINT_GRAPHICS,
                //                         drawInfo->pipelineLayout, 0, 1,
                //                         drawInfo->pDescriptorSet, 0, NULL);

                vkCmdDraw(cmdBuffer, 6, 1, 0, 0);

                vkCmdEndRenderPass(cmdBuffer);

                res = vkEndCommandBuffer(cmdBuffer);
                r_check_vkresult(res, "Failed to record command buffern");

                VkPipelineStageFlags waitStages[] = {
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                };
                VkSubmitInfo submitInfo = {
                        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                        .waitSemaphoreCount = 1,
                        .pWaitSemaphores
                        = r_state.imageAvailableSemaphore + currentFrame,
                        .pWaitDstStageMask = waitStages,
                        .commandBufferCount = 1,
                        .pCommandBuffers = &cmdBuffer,
                        .signalSemaphoreCount = 1,
                        .pSignalSemaphores
                        = r_state.renderFinishedSemaphore + currentFrame,
                };

                res = vkQueueSubmit(r_state.graphicsQueue, 1, &submitInfo,
                                    r_state.inflightFence[currentFrame]);
                r_check_vkresult(res, "Failed to sumbit draw command buffer");

                VkPresentInfoKHR presentInfo = {
                        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                        .waitSemaphoreCount = 1,
                        .pWaitSemaphores
                        = r_state.renderFinishedSemaphore + currentFrame,
                        .swapchainCount = 1,
                        .pSwapchains = &r_state.swapchain,
                        .pImageIndices = &imageIdx,
                };
                vkQueuePresentKHR(r_state.presentQueue, &presentInfo);

                r_state.currentFrame
                    = (currentFrame + 1) % r_state.maxFramesInFlight;

                glfwPollEvents();
        }

        // Clean up
        vkDeviceWaitIdle(r_state.device);

        vkDestroyPipelineLayout(r_state.device, r_state.pipelineLayout, 0);
        vkDestroyPipeline(r_state.device, r_state.pipeline, 0);
        vkDestroyBuffer(r_state.device, r_state.vertexBuffer, 0);
        vkFreeMemory(r_state.device, r_state.vertexMemory, 0);
        vkDestroyCommandPool(r_state.device, r_state.commandPool, 0);

        vkDestroyRenderPass(r_state.device, r_state.renderPass, 0);
        for (u32 i = 0; i < r_state.maxFramesInFlight; i++) {
                vkDestroySemaphore(r_state.device,
                                   r_state.imageAvailableSemaphore[i], 0);
                vkDestroySemaphore(r_state.device,
                                   r_state.renderFinishedSemaphore[i], 0);
                vkDestroyFence(r_state.device, r_state.inflightFence[i], 0);
                vkDestroyFramebuffer(r_state.device,
                                     r_state.swapchainFramebuffers[i], 0);
                vkDestroyImageView(r_state.device,
                                   r_state.swapchainImageViews[i], 0);
        }
        vkDestroySwapchainKHR(r_state.device, r_state.swapchain, 0);
        vkDestroyDevice(r_state.device, 0);
        vkDestroySurfaceKHR(r_state.instance, r_state.surface, 0);
        if (enable_validation_layers) {
                destroy_debug_utils_messenger_ext(r_state.instance,
                                                  debug_messenger, 0);
        }
        vkDestroyInstance(r_state.instance, 0);
        glfwDestroyWindow(r_state.window);
}
