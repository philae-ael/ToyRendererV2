#include "swapchain.h"

#include <GLFW/glfw3.h>
#include <json/value.h>
#include <spdlog/spdlog.h>
#include <utils/cast.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>

#include "../registry.h"
#include "constants.h"
#include "debug.h"
#include "deletion_stack.h"
#include "device.h"
#include "utils.h"
#include "vkformat.h"  // IWYU pragma: keep

auto tr::renderer::Swapchain::compute_extent(GLFWwindow* window) const -> VkExtent2D {
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  }

  int width{};
  int height{};
  glfwGetFramebufferSize(window, &width, &height);

  VkExtent2D new_extent{
      utils::narrow_cast<uint32_t>(width),
      utils::narrow_cast<uint32_t>(height),
  };

  new_extent.width = std::clamp(new_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
  new_extent.height =
      std::clamp(new_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

  return new_extent;
}

auto tr::renderer::Swapchain::init_with_config(Lifetime& lifetime, SwapchainConfig config, const Device& device,
                                               const PhysicalDevice& physical_device, VkSurfaceKHR surface,
                                               GLFWwindow* window) -> Swapchain {
  Swapchain s{};
  s.config = config;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device.vk_physical_device, surface, &s.capabilities);

  {
    std::uint32_t surface_format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device.vk_physical_device, surface, &surface_format_count, nullptr);

    s.available_formats.resize(surface_format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device.vk_physical_device, surface, &surface_format_count,
                                         s.available_formats.data());
    std::optional<VkSurfaceFormatKHR> chosen_format;
    spdlog::trace("Available surface formats:");
    for (const auto& available_format : s.available_formats) {
      spdlog::trace("\tformat {} | colorSpace {}", available_format.format, available_format.colorSpace);
      if (available_format.format == VK_FORMAT_B8G8R8A8_UNORM &&
          available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        chosen_format = available_format;
      }
    }
    s.surface_format = chosen_format.value_or(s.available_formats[0]);
    spdlog::debug("Surface format chosen: format {} | colorSpace {}", s.surface_format.format,
                  s.surface_format.colorSpace);
  }

  {
    std::uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device.vk_physical_device, surface, &present_mode_count,
                                              nullptr);

    s.available_present_modes.resize(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device.vk_physical_device, surface, &present_mode_count,
                                              s.available_present_modes.data());
    std::optional<VkPresentModeKHR> chosen_present_mode;
    spdlog::trace("Available present modes:");
    for (const auto& available_present_mode : s.available_present_modes) {
      spdlog::trace("\t{}", available_present_mode);

      if (available_present_mode == config.prefered_present_mode) {
        chosen_present_mode = available_present_mode;
      }
    }
    s.present_mode = chosen_present_mode.value_or(s.available_present_modes[0]);
    spdlog::debug("Present Mode chosen: {}", s.present_mode);
  }

  s.extent = s.compute_extent(window);
  uint32_t const image_count =
      std::clamp<uint32_t>(MAX_FRAMES_IN_FLIGHT, s.capabilities.minImageCount,
                           s.capabilities.maxImageCount == 0 ? MAX_FRAMES_IN_FLIGHT : s.capabilities.maxImageCount);
  Registry::global()["screen"]["width"] = s.extent.width;
  Registry::global()["screen"]["height"] = s.extent.height;

  {
    VkSharingMode image_sharing_mode{};
    std::vector<std::uint32_t> sharing_queue_families;
    if (physical_device.queues.graphics_family == physical_device.queues.present_family) {
      image_sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    } else {
      image_sharing_mode = VK_SHARING_MODE_CONCURRENT;
      sharing_queue_families.push_back(physical_device.queues.graphics_family);
      sharing_queue_families.push_back(physical_device.queues.present_family);
    }

    const VkSwapchainCreateInfoKHR create_info{
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
    lifetime.tie(DeviceHandle::SwapchainKHR, s.vk_swapchain);
  }
  {
    std::uint32_t swapchain_image_count{};
    vkGetSwapchainImagesKHR(device.vk_device, s.vk_swapchain, &swapchain_image_count, nullptr);
    s.images.resize(swapchain_image_count);
    vkGetSwapchainImagesKHR(device.vk_device, s.vk_swapchain, &swapchain_image_count, s.images.data());
  }

  s.image_views.resize(s.images.size());
  for (std::size_t i = 0; i < s.images.size(); i++) {
    const VkImageViewCreateInfo image_view_create_info{
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
    lifetime.tie(DeviceHandle::ImageView, s.image_views[i]);

    set_debug_object_name(device.vk_device, VK_OBJECT_TYPE_IMAGE, s.images[i], std::format("Swapchain image {}", i));
    set_debug_object_name(device.vk_device, VK_OBJECT_TYPE_IMAGE_VIEW, s.image_views[i],
                          std::format("Swapchain view {}", i));
  }

  return s;
}
