#ifndef _GLYPH_H
#define _GLYPH_H

#include "vulkan/vk_platform.h"
#include "vulkan/vulkan_core.h"
#include <assert.h>
#include <stdbool.h>
#include <stdalign.h>
#include <stdio.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vk_enum_string_helper.h>

#include "matrix.h"

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
  Mat4 mvp;
} CanvasUniform;

typedef struct {
  VkImage image[MAX_FRAMES_IN_FLIGHT];
  VkDeviceMemory imageMemory[MAX_FRAMES_IN_FLIGHT];

  VkImageView imageView[MAX_FRAMES_IN_FLIGHT];
  VkSampler imageSampler;

  void *mappedStagingImages[MAX_FRAMES_IN_FLIGHT];
  VkBuffer stagingImageBuffers[MAX_FRAMES_IN_FLIGHT];
  VkDeviceMemory stagingImagesMemory[MAX_FRAMES_IN_FLIGHT];

  uint32_t size;
  uint32_t width;
  uint32_t height;
  uint8_t *data;

  VkDescriptorSetLayout descriptorSetLayout;
  VkDescriptorPool descriptorPool;
  VkDescriptorSet descriptorSets[MAX_FRAMES_IN_FLIGHT];
  VkPipelineLayout pipelineLayout;
  VkPipeline pipeline;
  VkBuffer vertexBuffer;
  VkDeviceMemory vertexBufferMemory;

  CanvasUniform uniform;
  VkBuffer uniformBuffers[MAX_FRAMES_IN_FLIGHT];
  VkDeviceMemory uniformBuffersMemory[MAX_FRAMES_IN_FLIGHT];
  void *uniformBuffersMapped[MAX_FRAMES_IN_FLIGHT];

  Vec3 pos;
  Vec3 scale;

  const char *filename;
  uint32_t fileCreated;
} Canvas;

typedef struct {
  Mat4 mvp;
  alignas(16) Vec3 color;
} ControlPanelUniform;

#define COLOR_HISTORY_LENGTH 16
#define CONTROL_PANEL_QUAD_COUNT 1 + COLOR_HISTORY_LENGTH + 7

typedef enum {
  CONTROL_PANEL_CURRENT_COLOR,
  CONTROL_PANEL_HISTORY_1,
  CONTROL_PANEL_HISTORY_2,
  CONTROL_PANEL_HISTORY_3,
  CONTROL_PANEL_HISTORY_4,
  CONTROL_PANEL_HISTORY_5,
  CONTROL_PANEL_HISTORY_6,
  CONTROL_PANEL_HISTORY_7,
  CONTROL_PANEL_HISTORY_8,
  CONTROL_PANEL_HISTORY_9,
  CONTROL_PANEL_HISTORY_10,
  CONTROL_PANEL_HISTORY_11,
  CONTROL_PANEL_HISTORY_12,
  CONTROL_PANEL_HISTORY_13,
  CONTROL_PANEL_HISTORY_14,
  CONTROL_PANEL_HISTORY_15,
  CONTROL_PANEL_HISTORY_16,
  CONTROL_PANEL_CREATOR_PREVIEW,
  CONTROL_PANEL_CREATOR_R_BAR,
  CONTROL_PANEL_CREATOR_R_BUTTON,
  CONTROL_PANEL_CREATOR_G_BAR,
  CONTROL_PANEL_CREATOR_G_BUTTON,
  CONTROL_PANEL_CREATOR_B_BAR,
  CONTROL_PANEL_CREATOR_B_BUTTON,
} GlyphControlPanelUniformIndex;

typedef struct {
  VkPipeline Pipeline;
  VkPipelineLayout pipelineLayout;
  VkPipeline circlePipeline;
  VkPipelineLayout circlePipelineLayout;

  VkBuffer vertexBuffer;
  VkDeviceMemory vertexMemory;
  VkDescriptorSetLayout descriptorSetLayout;
  VkDescriptorPool descriptorPool;
  VkDescriptorSet descriptorSets[MAX_FRAMES_IN_FLIGHT];
  VkBuffer uniformBuffers[MAX_FRAMES_IN_FLIGHT];
  VkDeviceMemory uniformsMemory[MAX_FRAMES_IN_FLIGHT];
  void *uniformsMapped[MAX_FRAMES_IN_FLIGHT];
  VkDeviceSize alignedUniformSize;
  Vec3 colors[COLOR_HISTORY_LENGTH + 1];
  ControlPanelUniform quadUniforms[CONTROL_PANEL_QUAD_COUNT];
  Vec3 scales[CONTROL_PANEL_QUAD_COUNT];
  Vec3 positions[CONTROL_PANEL_QUAD_COUNT];
  int32_t clicked;
} ControlPanel;

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
  VkCommandPool command_pool;
  VkCommandBuffer command_buffer[MAX_FRAMES_IN_FLIGHT];
  VkSemaphore image_available_semaphore[MAX_FRAMES_IN_FLIGHT];
  VkSemaphore render_finished_semaphore[MAX_FRAMES_IN_FLIGHT];
  VkFence inflight_fence[MAX_FRAMES_IN_FLIGHT];

  VkBuffer index_buffer;
  VkDeviceMemory index_buffer_memory;

  Canvas canvas;
  ControlPanel controlPanel;

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
                                     VkCommandPool pool,
                                     VkCommandBuffer buffer);


#endif // _GLYPH_H
