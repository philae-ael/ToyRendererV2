#pragma once

#include <vulkan/vulkan_core.h>

#include <span>

#include "device.h"
#include "instance.h"
#include "swapchain.h"

namespace tr {
namespace renderer {
struct Lifetime;
}  // namespace renderer
struct Options;
}  // namespace tr
struct GLFWwindow;

namespace tr::renderer {
struct VulkanContext {
  Instance instance;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  PhysicalDevice physical_device;
  Device device;
  Swapchain swapchain;

  // TODO: window is not needed, an extent is enough
  // For both
  static auto init(Lifetime& swapchain_lifetime, tr::Options& options,
                   std::span<const char*> required_instance_extensions, GLFWwindow* w) -> VulkanContext;
  void rebuild_swapchain(Lifetime& swapchain_lifetime, GLFWwindow* w);
};
}  // namespace tr::renderer
