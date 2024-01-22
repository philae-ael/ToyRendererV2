#include "context.h"

auto tr::renderer::VulkanContext::init(Lifetime& swapchain_lifetime, tr::Options& options,
                                       std::span<const char*> required_instance_extensions, GLFWwindow* w)
    -> VulkanContext {
  const auto instance = Instance::init(options, required_instance_extensions);
  const auto surface = Surface::init(instance.vk_instance, w);
  const auto device = Device::init(instance.vk_instance, surface);
  const auto swapchain = Swapchain::init_with_config(swapchain_lifetime,
                                                     {
                                                         options.config.prefered_present_mode,
                                                     },
                                                     device, surface, w);

  return {
      .instance = instance,
      .surface = surface,
      .device = device,
      .swapchain = swapchain,
  };
}

void tr::renderer::VulkanContext::rebuild_swapchain(Lifetime& swapchain_lifetime, GLFWwindow* w) {
  swapchain.reinit(swapchain_lifetime, device, surface, w);
}
