#include "glyph.h"

#include <stdio.h>

const uint32_t default_window_width = 1000;
const uint32_t default_window_height = 750;

static void framebuffer_resize_callback(GLFWwindow *window, int width,
                                        int height) {
        glyph_state *state = glfwGetWindowUserPointer(window);
        state->engine.framebuffer_resized = 1;
}

// Initizalize a GLFW window. Returns 1 on success, 0 on failure.
static int init_window(GlyphEngine *engine, const char *windowName,
                       uint32_t width, uint32_t height) {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        GLFWwindow *window
            = glfwCreateWindow(width, height, windowName, NULL, NULL);
        glfwSetWindowUserPointer(window, engine);
        glfwSetFramebufferSizeCallback(window, framebuffer_resize_callback);
        engine->window = window;

        return 1;
}

// Creates a surface for the WSI (window system integratoin).
// Returns 1 on success, 0 on failure.
static int create_surface(GlyphEngine *engine) {
        VkResult res = glfwCreateWindowSurface(engine->instance, engine->window,
                                               NULL, &engine->surface);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create surface: %s\n",
                        string_VkResult(res));
                return 0;
        }
        return 1;
}
