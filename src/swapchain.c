#include "glyph.h"

#include <stdio.h>
#include <stdlib.h>

swapchain_support_details_t query_swapchain_support(glyph_state *state,
                                                    VkPhysicalDevice device) {
        swapchain_support_details_t details = { 0 };
        VkSurfaceKHR surface = state->surface;

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

VkExtent2D choose_swap_extent(glyph_state *state,
                              VkSurfaceCapabilitiesKHR cap) {
        if (cap.currentExtent.width != UINT32_MAX) {
                return cap.currentExtent;
        }
        int width, height;
        glfwGetFramebufferSize(state->window, &width, &height);

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
static int create_swapchain(glyph_state *state) {
        swapchain_support_details_t support
            = query_swapchain_support(state, state->physical_device);

        VkSurfaceFormatKHR surface_format = choose_sc_surface_format(support);
        VkPresentModeKHR present_mode = choose_sc_present_mode(support);
        VkExtent2D extent = choose_swap_extent(state, support.capabilities);

        uint32_t image_count = support.capabilities.minImageCount + 1;
        if (support.capabilities.maxImageCount > 0
            && image_count > support.capabilities.maxImageCount) {
                image_count = support.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createinfo = {
                .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                .surface = state->surface,
                .minImageCount = image_count,
                .imageFormat = surface_format.format,
                .imageColorSpace = surface_format.colorSpace,
                .imageExtent = extent,
                .imageArrayLayers = 1,
                .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        };

        queue_family_indicies_t indicies
            = find_queue_families(state, state->physical_device);
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

        VkResult res = vkCreateSwapchainKHR(state->device, &createinfo, NULL,
                                            &state->swapchain);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create swapchain: %s\n",
                        string_VkResult(res));
                return 0;
        }

        vkGetSwapchainImagesKHR(state->device, state->swapchain,
                                &state->swapchain_images_count, NULL);
        state->swapchain_images
            = malloc(sizeof(VkImage) * state->swapchain_images_count);
        vkGetSwapchainImagesKHR(state->device, state->swapchain,
                                &state->swapchain_images_count,
                                state->swapchain_images);
        state->swapchain_format = surface_format.format;
        state->swapchain_extent = extent;

        return 1;
}

// Creates image views. Returns 1 on success, 0 on failure.
static int create_image_views(glyph_state *state) {
        state->swapchain_image_views_count = state->swapchain_images_count;
        state->swapchain_image_views
            = malloc(sizeof(VkImageView) * state->swapchain_image_views_count);

        for (uint32_t i = 0; i < state->swapchain_image_views_count; i++) {
                VkImageViewCreateInfo createinfo = {
                        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                        .image = state->swapchain_images[i],
                        .viewType = VK_IMAGE_VIEW_TYPE_2D,
                        .format = state->swapchain_format,
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

                VkResult res
                    = vkCreateImageView(state->device, &createinfo, NULL,
                                        &state->swapchain_image_views[i]);
                if (res != VK_SUCCESS) {
                        fprintf(stderr, "Failed to create image view: %s\n",
                                string_VkResult(res));
                        return 0;
                }
        }

        return 1;
}
// Create framebuffers. Returns 1 on success, 0 on failure.
static int create_framebuffers(glyph_state *state) {
        state->swapchain_framebuffer_count = state->swapchain_image_views_count;
        state->swapchain_framebuffers = malloc(
            sizeof(VkFramebuffer) * state->swapchain_framebuffer_count);

        for (uint32_t i = 0; i < state->swapchain_framebuffer_count; i++) {
                VkImageView attachments[] = {
                        state->swapchain_image_views[i],
                };

                VkFramebufferCreateInfo createinfo = {
                        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                        .renderPass = state->render_pass,
                        .attachmentCount = 1,
                        .pAttachments = attachments,
                        .width = state->swapchain_extent.width,
                        .height = state->swapchain_extent.height,
                        .layers = 1,
                };

                VkResult res
                    = vkCreateFramebuffer(state->device, &createinfo, NULL,
                                          state->swapchain_framebuffers + i);
                if (res != VK_SUCCESS) {
                        fprintf(stderr, "Failed to create framebuffer: %s\n",
                                string_VkResult(res));
                        return 0;
                }
        }

        return 1;
}

static void cleanup_swapchain(glyph_state *state) {
        for (uint32_t i = 0; i < state->swapchain_framebuffer_count; i++) {
                vkDestroyFramebuffer(state->device,
                                     state->swapchain_framebuffers[i], NULL);
        }
        for (uint32_t i = 0; i < state->swapchain_image_views_count; i++) {
                vkDestroyImageView(state->device,
                                   state->swapchain_image_views[i], NULL);
        }
        vkDestroySwapchainKHR(state->device, state->swapchain, NULL);
}

// Recreates swap chain. Returns 1 on success, 0 on failure.
static int recreate_swapchain(glyph_state *state) {
        int width = 0, height = 0;
        glfwGetFramebufferSize(state->window, &width, &height);
        while (width == 0 || height == 0) {
                glfwGetFramebufferSize(state->window, &width, &height);
                glfwWaitEvents();
        }

        vkDeviceWaitIdle(state->device);

        cleanup_swapchain(state);

        int res = create_swapchain(state);
        if (!res)
                return 0;

        res = create_image_views(state);
        if (!res)
                return 0;

        res = create_framebuffers(state);
        if (!res)
                return 0;

        return 1;
}
