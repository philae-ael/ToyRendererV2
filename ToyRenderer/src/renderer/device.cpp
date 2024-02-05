#include "device.h"

#include <spdlog/spdlog.h>
#include <utils/assert.h>
#include <utils/cast.h>
#include <utils/data/static_stack.h>
#include <utils/misc.h>
#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <set>
#include <utility>
#include <vector>

#include "constants.h"
#include "debug.h"
#include "utils.h"
#include "vkformat.h"  // IWYU pragma: keep

void inspect_physical_device(const VkPhysicalDeviceProperties& properties) {
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
}

auto check_device_extensions(VkPhysicalDevice physical_device) -> std::optional<std::set<std::string>> {
  std::uint32_t extension_count = 0;
  vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr);

  std::vector<VkExtensionProperties> available_extensions(extension_count);
  vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, available_extensions.data());

  return tr::renderer::check_extensions("devices", tr::renderer::REQUIRED_DEVICE_EXTENSIONS,
                                        tr::renderer::OPTIONAL_DEVICE_EXTENSIONS, available_extensions);
}

auto check_queues(VkPhysicalDevice physical_device, VkSurfaceKHR surface) -> std::optional<tr::renderer::QueuesInfo> {
  const std::vector<VkQueueFamilyProperties> queue_families = INLINE_LAMBDA {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, families.data());
    return families;
  };

  std::optional<std::size_t> graphics_family;
  std::optional<std::size_t> present_family;
  std::optional<std::size_t> transfert_family;
  for (std::size_t i = 0; i < queue_families.size(); i++) {
    const auto& queue_family = queue_families[i];

    if ((queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
      graphics_family = i;
    }

    if ((queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0) {
      transfert_family = i;
    }

    auto present_support = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, utils::narrow_cast<uint32_t>(i), surface, &present_support);
    if (present_support == VK_TRUE) {
      present_family = i;
    }
  }
  if (!(graphics_family.has_value() && present_family.has_value() && transfert_family.has_value())) {
    return std::nullopt;
  }

  return tr::renderer::QueuesInfo{
      .graphics_family = utils::narrow_cast<std::uint32_t>(*graphics_family),
      .present_family = utils::narrow_cast<std::uint32_t>(*present_family),
      .transfer_family = utils::narrow_cast<std::uint32_t>(*transfert_family),

      .graphics_family_properties = queue_families[*graphics_family],
      .present_family_properties = queue_families[*present_family],
      .transfer_family_properties = queue_families[*transfert_family],
  };
}

auto check_surface_formats(VkPhysicalDevice physical_device, VkSurfaceKHR surface) -> bool {
  std::uint32_t surface_format_count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count, nullptr);
  std::uint32_t present_mode_count = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr);

  return surface_format_count != 0 && present_mode_count != 0;
}

auto gather_physical_device_infos(VkPhysicalDevice physical_device, VkSurfaceKHR surface)
    -> std::optional<tr::renderer::PhysicalDevice> {
  tr::renderer::PhysicalDevice infos;
  infos.vk_physical_device = physical_device;

  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(physical_device, &properties);

  inspect_physical_device(properties);
  if (auto extensions = check_device_extensions(physical_device); extensions) {
    infos.extensions = std::move(*extensions);
  } else {
    return std::nullopt;
  }
  if (const auto queues_info = check_queues(physical_device, surface); queues_info) {
    infos.queues = *queues_info;
  } else {
    return std::nullopt;
  }
  if (!check_surface_formats(physical_device, surface)) {
    return std::nullopt;
  }

  vkGetPhysicalDeviceProperties(physical_device, &infos.device_properties);
  vkGetPhysicalDeviceMemoryProperties(physical_device, &infos.memory_properties);
  // We take the 1st one suitable
  spdlog::debug("Device is suitable!");

  return infos;
}

auto tr::renderer::PhysicalDevice::init(VkInstance instance, VkSurfaceKHR surface) -> PhysicalDevice {
  const std::vector<VkPhysicalDevice> available_devices = INLINE_LAMBDA {
    std::uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    TR_ASSERT(device_count > 0, "no physical device has been found");

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
    return devices;
  };

  spdlog::debug("Available devices");
  for (const auto& physical_device : available_devices) {
    const auto physical_device_infos = gather_physical_device_infos(physical_device, surface);

    if (physical_device_infos) {
      return *physical_device_infos;
    }
  }

  TR_ASSERT(false, "could not find any suitable physical device");
}

auto tr::renderer::Device::init(const PhysicalDevice& infos) -> Device {
  Device device;
  const std::set<std::uint32_t> queue_families{
      infos.queues.graphics_family,
      infos.queues.present_family,
      infos.queues.transfer_family,
  };

  utils::data::static_stack<VkDeviceQueueCreateInfo, 3> queue_create_infos;
  const float queue_priority = 1.0;
  for (auto queue_family : queue_families) {
    queue_create_infos.push_back({
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueFamilyIndex = queue_family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    });
  }

  VkPhysicalDeviceVulkan12Features vulkan12_features{};
  vulkan12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  vulkan12_features.pNext = nullptr;
  vulkan12_features.hostQueryReset = VK_TRUE;
  vulkan12_features.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
  vulkan12_features.descriptorBindingPartiallyBound = VK_TRUE;
  vulkan12_features.runtimeDescriptorArray = VK_TRUE;

  VkPhysicalDeviceVulkan13Features vulkan13_features{};
  vulkan13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
  vulkan13_features.pNext = &vulkan12_features;
  vulkan13_features.synchronization2 = VK_TRUE;
  vulkan13_features.dynamicRendering = VK_TRUE;

  const auto r = infos.extensions | std::views::transform(&std::string::c_str);
  const std::vector<const char*> extensions{r.begin(), r.end()};

  const VkDeviceCreateInfo device_create_info{
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = &vulkan13_features,
      .flags = 0,
      .queueCreateInfoCount = utils::narrow_cast<std::uint32_t>(queue_create_infos.size()),
      .pQueueCreateInfos = queue_create_infos.data(),
      .enabledLayerCount = 0,
      .ppEnabledLayerNames = nullptr,
      .enabledExtensionCount = utils::narrow_cast<std::uint32_t>(extensions.size()),
      .ppEnabledExtensionNames = extensions.data(),
      .pEnabledFeatures = nullptr,
  };

  VkResult result = vkCreateDevice(infos.vk_physical_device, &device_create_info, nullptr, &device.vk_device);
  TR_ASSERT(result == VkResult::VK_SUCCESS, "failed to create logical device, got error code {}", result);

  // WHEN graphics_queue / present_queue or transfert_family are the same we should use multiple queue indexes?
  vkGetDeviceQueue(device.vk_device, infos.queues.graphics_family, 0, &device.graphics_queue);
  vkGetDeviceQueue(device.vk_device, infos.queues.present_family, 0, &device.present_queue);
  vkGetDeviceQueue(device.vk_device, infos.queues.transfer_family, 0, &device.transfer_queue);

  set_debug_object_name(device.vk_device, VK_OBJECT_TYPE_QUEUE, device.graphics_queue, "graphics queue");
  set_debug_object_name(device.vk_device, VK_OBJECT_TYPE_QUEUE, device.present_queue, "present queue");
  set_debug_object_name(device.vk_device, VK_OBJECT_TYPE_QUEUE, device.transfer_queue, "transfer queue");

  return device;
}
