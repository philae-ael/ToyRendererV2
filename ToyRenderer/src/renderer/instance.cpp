#include "instance.h"

#include <utils/assert.h>
#include <vulkan/vulkan_core.h>

#include "utils.h"
#include "utils/cast.h"

const std::array<const char*, 1> wanted_validation_layers{
    "VK_LAYER_KHRONOS_validation",
};

VKAPI_ATTR auto VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
               VkDebugUtilsMessageTypeFlagsEXT messageType,
               const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
               void* /*pUserData*/) -> VkBool32;

auto tr::renderer::Instance::init(
    const tr::Options& options,
    std::span<const char*> required_vulkan_instance_extensions)
    -> tr::renderer::Instance {
  Instance instance;

  std::vector<const char*> required_extensions{
      required_vulkan_instance_extensions.begin(),
      required_vulkan_instance_extensions.end(),
  };

  std::vector<const char*> validation_layers{};
  if (options.debug.validations_layers) {
    required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    std::uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    std::vector<VkLayerProperties> layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, layers.data());

    std::size_t satisfied_layer_count = 0;
    spdlog::trace("Available layers:");
    for (const auto& layer : layers) {
      spdlog::trace("\t{}", layer.layerName);

      for (const auto& wanted_layer : wanted_validation_layers) {
        if (std::strcmp(wanted_layer, layer.layerName) == 0) {
          satisfied_layer_count += 1;
          validation_layers.push_back(wanted_layer);
        }
      }
    }

    spdlog::trace("Wanted layers:");
    for (const auto& wanted_layer : wanted_validation_layers) {
      spdlog::trace("\t{}", wanted_layer);
    }

    if (wanted_validation_layers.size() != satisfied_layer_count) {
      spdlog::warn("{} wanted layers are satisfied out of {}",
                   satisfied_layer_count, wanted_validation_layers.size());
    }
  }

  {
    std::uint32_t extension_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);

    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count,
                                           available_extensions.data());

    TR_ASSERT(
        check_extensions("instance", required_extensions, available_extensions),
        "Required extensions are not satisfied");
  }

  {
    VkDebugUtilsMessengerCreateInfoEXT createInfoDebugUtils{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = nullptr,
        .flags = 0,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debug_callback,
        .pUserData = nullptr,
    };

    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "ToyRenderer",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "ToyRenderer",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };

    VkInstanceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount =
            utils::narrow_cast<std::uint32_t>(validation_layers.size()),
        .ppEnabledLayerNames = validation_layers.data(),
        .enabledExtensionCount =
            utils::narrow_cast<std::uint32_t>(required_extensions.size()),
        .ppEnabledExtensionNames = required_extensions.data(),
    };

    if (options.debug.validations_layers) {
      createInfo.pNext = &createInfoDebugUtils;
    }

    VK_UNWRAP(vkCreateInstance, &createInfo, nullptr, &instance.vk_instance);
    load_extensions(instance.vk_instance);

    if (options.debug.validations_layers) {
      VK_UNWRAP(vkCreateDebugUtilsMessengerEXT, instance.vk_instance,
                &createInfoDebugUtils, nullptr,
                &instance.vk_debug_utils_messenger_ext);
    }
  }
  return instance;
}

VKAPI_ATTR auto VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
               VkDebugUtilsMessageTypeFlagsEXT messageType,
               const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
               void* /*pUserData*/) -> VkBool32 {
  spdlog::level::level_enum level{spdlog::level::warn};

  switch (messageSeverity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
      level = spdlog::level::trace;
      break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
      level = spdlog::level::info;
      break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
      level = spdlog::level::warn;
      break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
      level = spdlog::level::err;
      break;
    default:
      break;
  }

  const char* reason{""};

  switch (messageType) {
    case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
      reason = "general";
      break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
      reason = "validation";
      break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
      reason = "performance";
      break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT:
      reason = "device address binding";
      break;
    default:
      reason = "unknown";
      break;
  }

  spdlog::log(level, "[{}] - ({}:{}): {}", reason,
              pCallbackData->pMessageIdName, pCallbackData->pMessageIdName,
              pCallbackData->pMessage);

  if (pCallbackData->objectCount > 0) {
    const char* objectName = "unnamed object";
    if (pCallbackData->pObjects[0].pObjectName != nullptr) {
      objectName = pCallbackData->pObjects[0].pObjectName;
    }

    spdlog::log(
        level,
        "[{}] - ({}:{}): 1st object affected {} "
        "(type: {}, name:\"{}\")",
        reason, pCallbackData->pMessageIdName, pCallbackData->pMessageIdName,
        pCallbackData->pObjects[0].objectHandle,
        tr::renderer::vkObjectTypeName(pCallbackData->pObjects[0].objectType),
        objectName);
  }

  return VK_FALSE;
}
