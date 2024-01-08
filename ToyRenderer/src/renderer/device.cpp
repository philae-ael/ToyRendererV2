#include "device.h"

#include <GLFW/glfw3.h>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <utils/assert.h>
#include <utils/cast.h>
#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <set>
#include <vector>

#include "utils.h"

const std::array<const char*, 1> device_extension{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

void init_physical_device(tr::renderer::Device& device, VkInstance instance, VkSurfaceKHR surface) {
  std::uint32_t device_count = 0;
  vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
  TR_ASSERT(device_count > 0, "no physical device has been found");

  std::vector<VkPhysicalDevice> available_devices(device_count);
  vkEnumeratePhysicalDevices(instance, &device_count, available_devices.data());
  spdlog::debug("Available devices");
  for (const auto& physical_device : available_devices) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    const char* device_type = "other";
    switch (properties.deviceType) {
      case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        device_type = "integrated GPU";
        break;
      case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        device_type = "discrete GPU";
        break;
      case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        device_type = "virtual CPU";
        break;
      case VK_PHYSICAL_DEVICE_TYPE_CPU:
        device_type = "CPU";
        break;
      default:
        break;
    }
    spdlog::debug("\tName: {} ({})", properties.deviceName, device_type);

    {
      std::uint32_t extension_count = 0;
      vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr);

      std::vector<VkExtensionProperties> available_extensions(extension_count);
      vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, available_extensions.data());
      if (!tr::renderer::check_extensions("extensions", device_extension, available_extensions)) {
        continue;
      }
    }
    {
      std::uint32_t surface_format_count = 0;
      vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count, nullptr);
      std::uint32_t present_mode_count = 0;
      vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr);
      if (surface_format_count == 0 || present_mode_count == 0) {
        continue;
      }
    }

    {
      uint32_t queue_family_count = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
      std::vector<VkQueueFamilyProperties> queue_families{queue_family_count};
      vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());
      std::optional<std::size_t> graphics_family;
      std::optional<std::size_t> present_family;
      for (std::size_t i = 0; i < queue_family_count; i++) {
        const auto& queue_family = queue_families[i];

        if ((queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
          graphics_family = i;
        }

        auto present_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_support);
        if (present_support == VK_TRUE) {
          present_family = i;
        }
      }
      if (!graphics_family.has_value() || !present_family.has_value()) {
        continue;
      }

      device.queues.graphics_family = utils::narrow_cast<std::uint32_t>(*graphics_family);
      device.queues.graphics_family_properties = queue_families[*graphics_family];
      device.queues.present_family = utils::narrow_cast<std::uint32_t>(*present_family);
      device.queues.present_family_properties = queue_families[*present_family];
    }

    // We take the 1st one suitable
    spdlog::debug("\tDevice is suitable!");
    device.physical_device = physical_device;
    vkGetPhysicalDeviceProperties(device.physical_device, &device.device_properties);
    break;
  }

  TR_ASSERT(device.physical_device != VK_NULL_HANDLE, "could not find any suitable physical device");
}

void init_device(tr::renderer::Device& device) {
  std::set<std::uint32_t> queue_families{
      device.queues.graphics_family,
      device.queues.present_family,
  };

  std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
  for (auto queue_family : queue_families) {
    float queue_priority = 1.0;
    queue_create_infos.push_back({
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueFamilyIndex = queue_family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    });
  }

  VkPhysicalDeviceVulkan12Features enabled_features12{};
  enabled_features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
  enabled_features12.hostQueryReset = VK_TRUE;

  VkDeviceCreateInfo device_create_info{
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = &enabled_features12,
      .flags = 0,
      .queueCreateInfoCount = utils::narrow_cast<std::uint32_t>(queue_create_infos.size()),
      .pQueueCreateInfos = queue_create_infos.data(),
      .enabledLayerCount = 0,
      .ppEnabledLayerNames = nullptr,
      .enabledExtensionCount = utils::narrow_cast<std::uint32_t>(device_extension.size()),
      .ppEnabledExtensionNames = device_extension.data(),
      .pEnabledFeatures = nullptr,
  };

  VkResult result = vkCreateDevice(device.physical_device, &device_create_info, nullptr, &device.vk_device);
  TR_ASSERT(result == VkResult::VK_SUCCESS, "failed to create logical device, got error code {}", result);

  vkGetDeviceQueue(device.vk_device, device.queues.graphics_family, 0, &device.queues.graphics_queue);
  vkGetDeviceQueue(device.vk_device, device.queues.present_family, 0, &device.queues.present_queue);
}

auto tr::renderer::Device::init(VkInstance instance, VkSurfaceKHR surface) -> Device {
  Device device;
  init_physical_device(device, instance, surface);
  init_device(device);
  return device;
}
