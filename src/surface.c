#include "glyph.h"

#include <stdio.h>

const uint32_t default_window_width = 1000;
const uint32_t default_window_height = 750;

static void framebuffer_resize_callback(GLFWwindow *window, int width,
                                        int height) {
        glyph_state *state = glfwGetWindowUserPointer(window);
        state->framebuffer_resized = 1;
}

// Initizalize a GLFW window. Returns 1 on success, 0 on failure.
static int init_window(glyph_state *state) {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        GLFWwindow *window = glfwCreateWindow(
            default_window_width, default_window_height, "glyph", NULL, NULL);
        glfwSetWindowUserPointer(window, state);
        glfwSetFramebufferSizeCallback(window, framebuffer_resize_callback);
        state->window = window;

        return 1;
}

// Creates a surface for the WSI (window system integratoin).
// Returns 1 on success, 0 on failure.
static int create_surface(glyph_state *state) {
        VkResult res = glfwCreateWindowSurface(state->instance, state->window,
                                               NULL, &state->surface);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create surface: %s\n",
                        string_VkResult(res));
                return 0;
        }
        return 1;
}
