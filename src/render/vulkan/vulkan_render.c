/* Vulkan validation layer and debug extension */
#ifdef NDEBUG
static u32 enable_validation_layers = 0;
#else
static u32 enable_validation_layers = 1;
#endif

static const char *validation_layers[] = {
        "VK_LAYER_KHRONOS_validation",
};
uint32_t validation_layer_count
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

static void r_render_init(void) {
        r_state.arena = make_arena(0xFF00);

        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        String8 windowName = string8_lit("vulkan start");
        GLFWwindow *window = glfwCreateWindow(
            r_state.width, r_state.height, (const char *)windowName.data, 0, 0);
        glfwSetFramebufferSizeCallback(window, framebuffer_resize_callback);

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

        VkSurfaceKHR surface = 0;
        res = glfwCreateWindowSurface(r_state.instance, window, 0, &surface);
        r_check_vkresult(res, "Failed to create surface");
}
