#ifndef _GLYPH_H
#define _GLYPH_H

#include "vulkan/vk_platform.h"
#include "vulkan/vulkan_core.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vk_enum_string_helper.h>

#ifdef NDEBUG
const uint32_t enable_validation_layers = 0;
#else
const uint32_t enable_validation_layers = 1;
#endif

const char *validation_layers[] = {
    "VK_LAYER_KHRONOS_validation",
};
uint32_t validation_layer_count =
    sizeof(validation_layers) / sizeof(validation_layers[0]);

#define MAX_FRAMES_IN_FLIGHT 2

typedef struct {
  uint32_t current_frame;

  VkInstance instance;
  VkDevice device;
  VkPhysicalDevice physical_device;
  VkQueue graphics_queue;
  VkQueue presentation_queue;
  GLFWwindow *window;
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;
  uint32_t swapchain_images_count;
  VkImage *swapchain_images;
  VkFormat swapchain_format;
  VkExtent2D swapchain_extent;
  uint32_t swapchain_image_views_count;
  VkImageView *swapchain_image_views;
  uint32_t swapchain_framebuffer_count;
  VkFramebuffer *swapchain_framebuffers;
  VkRenderPass render_pass;
  VkDescriptorSetLayout descriptor_set_layout;
  VkDescriptorPool descriptor_pool;
  VkDescriptorSet descriptor_sets[MAX_FRAMES_IN_FLIGHT];
  VkPipelineLayout pipeline_layout;
  VkPipeline graphics_pipeline;
  VkCommandPool command_pool;
  VkCommandBuffer command_buffer[MAX_FRAMES_IN_FLIGHT];
  VkSemaphore image_available_semaphore[MAX_FRAMES_IN_FLIGHT];
  VkSemaphore render_finished_semaphore[MAX_FRAMES_IN_FLIGHT];
  VkFence inflight_fence[MAX_FRAMES_IN_FLIGHT];
  VkBuffer vertex_buffer;
  VkDeviceMemory vertex_buffer_memory;
  VkBuffer index_buffer;
  VkDeviceMemory index_buffer_memory;
  VkImage texture_image;
  VkDeviceMemory texture_memory;
  VkImageView texture_view;
  VkSampler texture_sampler;
  VkBuffer uniform_buffers[MAX_FRAMES_IN_FLIGHT];
  VkDeviceMemory uniform_buffers_memory[MAX_FRAMES_IN_FLIGHT];
  void *uniform_buffers_mapped[MAX_FRAMES_IN_FLIGHT];

  uint32_t framebuffer_resized;
} glyph_state;

typedef struct {
  uint32_t index;
  uint32_t valid;
} queue_family_index_t;

typedef struct {
  queue_family_index_t graphics;
  queue_family_index_t presentation;
} queue_family_indicies_t;

static queue_family_indicies_t find_queue_families(glyph_state *state,
                                                   VkPhysicalDevice device);

typedef struct {
  VkSurfaceCapabilitiesKHR capabilities;
  VkSurfaceFormatKHR *formats;
  VkPresentModeKHR *present_modes;
  uint32_t formats_count;
  uint32_t present_modes_count;
} swapchain_support_details_t;

static swapchain_support_details_t
query_swapchain_support(glyph_state *state, VkPhysicalDevice device);

static VkCommandBuffer begin_single_time_commands(VkDevice device,
                                                  VkCommandPool pool);
static void end_single_time_commands(VkDevice device, VkQueue graphics_queue,
                                     VkCommandPool pool, VkCommandBuffer buffer);
static VkVertexInputBindingDescription get_vertex_binding_desc(void);
static VkVertexInputAttributeDescription get_vertex_attr_desc_pos(void);
static VkVertexInputAttributeDescription get_vertex_attr_desc_tex_coord(void);
static void update_uniform_buffer(glyph_state *state, uint32_t currentFrame);

#endif // _GLYPH_H
