#include "swapchain.h"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#include <cstdint>

#include "device.h"
#include "utils.h"
#include "utils/cast.h"

auto tr::renderer::Swapchain::compute_extent(GLFWwindow* window) const
    -> VkExtent2D {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  }

  int width{};
  int height{};
  glfwGetFramebufferSize(window, &width, &height);

  VkExtent2D extent{
      utils::narrow_cast<uint32_t>(width),
      utils::narrow_cast<uint32_t>(height),
  };

  extent.width = std::clamp(extent.width, capabilities.minImageExtent.width,
                            capabilities.maxImageExtent.width);
  extent.height = std::clamp(extent.height, capabilities.minImageExtent.height,
                             capabilities.maxImageExtent.height);

  return extent;
}

auto tr::renderer::Swapchain::init(tr::renderer::Device& device,
                                   VkSurfaceKHR surface, GLFWwindow* window)
    -> Swapchain {
  Swapchain swapchain;
  VkPhysicalDevice physical_device = device.physical_device;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface,
                                            &swapchain.capabilities);

  std::uint32_t surface_format_count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface,
                                       &surface_format_count, nullptr);

  swapchain.available_formats.resize(surface_format_count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface,
                                       &surface_format_count,
                                       swapchain.available_formats.data());

  std::uint32_t present_mode_count = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface,
                                            &present_mode_count, nullptr);

  swapchain.available_present_modes.resize(present_mode_count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(
      physical_device, surface, &present_mode_count,
      swapchain.available_present_modes.data());

  std::optional<VkSurfaceFormatKHR> chosen_format;
  for (const auto& available_format : swapchain.available_formats) {
    if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB &&
        available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      chosen_format = available_format;
    }
  }
  swapchain.surface_format =
      chosen_format.value_or(swapchain.available_formats[0]);

  std::optional<VkPresentModeKHR> chosen_present_mode;
  for (const auto& available_present_mode : swapchain.available_present_modes) {
    if (available_present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
      chosen_present_mode = available_present_mode;
    }
  }
  swapchain.present_mode =
      chosen_present_mode.value_or(swapchain.available_present_modes[0]);

  swapchain.extent = swapchain.compute_extent(window);
  uint32_t image_count = swapchain.capabilities.minImageCount + 1;
  if (swapchain.capabilities.maxImageCount != 0 &&
      swapchain.capabilities.maxImageCount < image_count) {
    image_count = swapchain.capabilities.maxImageCount;
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
      .imageFormat = swapchain.surface_format.format,
      .imageColorSpace = swapchain.surface_format.colorSpace,
      .imageExtent = swapchain.extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .imageSharingMode = image_sharing_mode,
      .queueFamilyIndexCount =
          utils::narrow_cast<std::uint32_t>(sharing_queue_families.size()),
      .pQueueFamilyIndices = sharing_queue_families.data(),
      .preTransform = swapchain.capabilities.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = swapchain.present_mode,
      .clipped = VK_TRUE,
      .oldSwapchain = swapchain.vk_swapchain,
  };

  VK_UNWRAP(vkCreateSwapchainKHR, device.vk_device, &create_info, nullptr,
            &swapchain.vk_swapchain);
  {
    std::uint32_t image_count{};
    vkGetSwapchainImagesKHR(device.vk_device, swapchain.vk_swapchain,
                            &image_count, nullptr);
    swapchain.images.resize(image_count);
    vkGetSwapchainImagesKHR(device.vk_device, swapchain.vk_swapchain,
                            &image_count, swapchain.images.data());
  }

  swapchain.image_views.resize(swapchain.images.size());
  for (std::size_t i = 0; i < swapchain.images.size(); i++) {
    VkImageViewCreateInfo image_view_create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = swapchain.images[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = swapchain.surface_format.format,
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

    VK_UNWRAP(vkCreateImageView, device.vk_device, &image_view_create_info,
              nullptr, &swapchain.image_views[i]);
  }
  return swapchain;
}

auto tr::renderer::Swapchain::acquire_next_frame(tr::renderer::Device& device,
                                                 FrameSynchro synchro) const
    -> tr::renderer::Frame {
  VK_UNWRAP(vkWaitForFences, device.vk_device, 1, &synchro.render_fence,
            VK_TRUE, 1000000000);
  VK_UNWRAP(vkResetFences, device.vk_device, 1, &synchro.render_fence);

  Frame frame{
      .swapchain_image_index = 0,
      .synchro = synchro,
  };
  VK_UNWRAP(vkAcquireNextImageKHR, device.vk_device, vk_swapchain, 1000000000,
            synchro.present_semaphore, VK_NULL_HANDLE,
            &frame.swapchain_image_index);

  return frame;
}

auto tr::renderer::FrameSynchro::init(VkDevice device) -> FrameSynchro {
  FrameSynchro synchro{};
  VkFenceCreateInfo fence_create_info{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };
  VK_UNWRAP(vkCreateFence, device, &fence_create_info, nullptr,
            &synchro.render_fence);

  VkSemaphoreCreateInfo semaphore_create_info{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };
  VK_UNWRAP(vkCreateSemaphore, device, &semaphore_create_info, nullptr,
            &synchro.render_semaphore);
  VK_UNWRAP(vkCreateSemaphore, device, &semaphore_create_info, nullptr,
            &synchro.present_semaphore);
  return synchro;
}
