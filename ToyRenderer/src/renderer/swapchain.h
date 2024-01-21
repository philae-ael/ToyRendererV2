#pragma once

#include <utils/types.h>
#include <vulkan/vulkan_core.h>

#include <vector>

#include "deletion_stack.h"
#include "device.h"
#include "surface.h"

namespace tr::renderer {

// TODO: MOVE THAT

struct Swapchain {
  struct SwapchainConfig;
  void reinit(Lifetime &lifetime, Device &device, VkSurfaceKHR surface, GLFWwindow *window) {
    *this = init_with_config(lifetime, config, device, surface, window);
  }
  static auto init_with_config(Lifetime &lifetime, SwapchainConfig config, const Device &device, VkSurfaceKHR surface,
                               GLFWwindow *window) -> Swapchain;

  VkSwapchainKHR vk_swapchain = VK_NULL_HANDLE;

  VkExtent2D extent{};
  VkSurfaceFormatKHR surface_format{};
  VkPresentModeKHR present_mode{};
  VkSurfaceCapabilitiesKHR capabilities{};

  std::vector<VkSurfaceFormatKHR> available_formats;
  std::vector<VkPresentModeKHR> available_present_modes;

  std::vector<VkImage> images;
  std::vector<VkImageView> image_views;

  struct SwapchainConfig {
    VkPresentModeKHR prefered_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    float internal_resolution_scale = 0.5;
  } config;

 private:
  auto compute_extent(GLFWwindow *window) const -> VkExtent2D;
};
}  // namespace tr::renderer
