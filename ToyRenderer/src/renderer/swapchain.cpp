#include "swapchain.h"

#include <GLFW/glfw3.h>
#include <utils/cast.h>
#include <vulkan/vulkan_core.h>

#include <cstdint>

#include "device.h"
#include "utils.h"

auto tr::renderer::Swapchain::compute_extent(GLFWwindow* window) const -> VkExtent2D {
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  }

  int width{};
  int height{};
  glfwGetFramebufferSize(window, &width, &height);

  VkExtent2D extent{
      utils::narrow_cast<uint32_t>(width),
      utils::narrow_cast<uint32_t>(height),
  };

  extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
  extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

  return extent;
}

auto tr::renderer::Swapchain::init_with_config(SwapchainConfig config, tr::renderer::Device& device,
                                               VkSurfaceKHR surface, GLFWwindow* window) -> Swapchain {
  // TODO: better, it works but not very well
  Swapchain s{};
  s.config = config;

  VkPhysicalDevice physical_device = device.physical_device;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &s.capabilities);

  std::uint32_t surface_format_count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count, nullptr);

  s.available_formats.resize(surface_format_count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count, s.available_formats.data());

  std::uint32_t present_mode_count = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr);

  s.available_present_modes.resize(present_mode_count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count,
                                            s.available_present_modes.data());

  std::optional<VkSurfaceFormatKHR> chosen_format;
  for (const auto& available_format : s.available_formats) {
    if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB &&
        available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      chosen_format = available_format;
    }
  }
  s.surface_format = chosen_format.value_or(s.available_formats[0]);

  std::optional<VkPresentModeKHR> chosen_present_mode;
  for (const auto& available_present_mode : s.available_present_modes) {
    if (available_present_mode == config.prefered_present_mode) {
      chosen_present_mode = available_present_mode;
    }
  }
  s.present_mode = chosen_present_mode.value_or(s.available_present_modes[0]);

  s.extent = s.compute_extent(window);
  uint32_t image_count = s.capabilities.minImageCount + 1;
  if (s.capabilities.maxImageCount != 0 && s.capabilities.maxImageCount < image_count) {
    image_count = s.capabilities.maxImageCount;
  }

  VkSharingMode image_sharing_mode{};
  std::vector<std::uint32_t> sharing_queue_families;
  if (device.queues.graphics_family == device.queues.present_family) {
    image_sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
  } else {
    image_sharing_mode = VK_SHARING_MODE_CONCURRENT;
    sharing_queue_families.push_back(device.queues.graphics_family);
    sharing_queue_families.push_back(device.queues.present_family);
  }

  VkSwapchainCreateInfoKHR create_info{
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .pNext = nullptr,
      .flags = 0,
      .surface = surface,
      .minImageCount = image_count,
      .imageFormat = s.surface_format.format,
      .imageColorSpace = s.surface_format.colorSpace,
      .imageExtent = s.extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .imageSharingMode = image_sharing_mode,
      .queueFamilyIndexCount = utils::narrow_cast<std::uint32_t>(sharing_queue_families.size()),
      .pQueueFamilyIndices = sharing_queue_families.data(),
      .preTransform = s.capabilities.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = s.present_mode,
      .clipped = VK_TRUE,
      .oldSwapchain = s.vk_swapchain,
  };

  VK_UNWRAP(vkCreateSwapchainKHR, device.vk_device, &create_info, nullptr, &s.vk_swapchain);
  {
    std::uint32_t image_count{};
    vkGetSwapchainImagesKHR(device.vk_device, s.vk_swapchain, &image_count, nullptr);
    s.images.resize(image_count);
    vkGetSwapchainImagesKHR(device.vk_device, s.vk_swapchain, &image_count, s.images.data());
  }

  s.image_views.resize(s.images.size());
  for (std::size_t i = 0; i < s.images.size(); i++) {
    VkImageViewCreateInfo image_view_create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = s.images[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = s.surface_format.format,
        .components =
            {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    VK_UNWRAP(vkCreateImageView, device.vk_device, &image_view_create_info, nullptr, &s.image_views[i]);
  }
  return s;
}

auto tr::renderer::Swapchain::init(tr::Options& options, tr::renderer::Device& device, VkSurfaceKHR surface,
                                   GLFWwindow* window) -> Swapchain {
  return init_with_config({options.config.prefered_present_mode}, device, surface, window);
}

auto tr::renderer::Swapchain::acquire_next_frame(tr::renderer::Device& device, Frame* frame) const -> VkResult {
  return vkAcquireNextImageKHR(device.vk_device, vk_swapchain, 1000000000, frame->synchro.present_semaphore,
                               VK_NULL_HANDLE, &frame->swapchain_image_index);
}

auto tr::renderer::FrameSynchro::init(VkDevice device) -> FrameSynchro {
  FrameSynchro synchro{};
  VkFenceCreateInfo fence_create_info{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };
  VK_UNWRAP(vkCreateFence, device, &fence_create_info, nullptr, &synchro.render_fence);

  VkSemaphoreCreateInfo semaphore_create_info{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };
  VK_UNWRAP(vkCreateSemaphore, device, &semaphore_create_info, nullptr, &synchro.render_semaphore);
  VK_UNWRAP(vkCreateSemaphore, device, &semaphore_create_info, nullptr, &synchro.present_semaphore);
  return synchro;
}
