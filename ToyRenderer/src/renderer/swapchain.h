#pragma once

#include <vulkan/vulkan_core.h>

#include <vector>

namespace tr {
namespace renderer {
struct Device;
struct Lifetime;
struct PhysicalDevice;
}  // namespace renderer
}  // namespace tr
struct GLFWwindow;

namespace tr::renderer {

// TODO: MOVE THAT

struct Swapchain {
  struct SwapchainConfig;

  void reinit(Lifetime& lifetime, const Device& device, const PhysicalDevice& physical_device, VkSurfaceKHR surface,
              GLFWwindow* window) {
    *this = init_with_config(lifetime, config, device, physical_device, surface, window);
  }
  static auto init_with_config(Lifetime& lifetime, SwapchainConfig config, const tr::renderer::Device& device,
                               const tr::renderer::PhysicalDevice& physical_device, VkSurfaceKHR surface,
                               GLFWwindow* window) -> Swapchain;

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
  } config;

 private:
  auto compute_extent(GLFWwindow* window) const -> VkExtent2D;
};
}  // namespace tr::renderer
