#include "instance.h"

#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <utils/assert.h>
#include <utils/cast.h>
#include <utils/misc.h>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

#include "../options.h"
#include "constants.h"
#include "extensions.h"
#include "utils.h"
#include "vkformat.h"  // IWYU pragma: keep

VKAPI_ATTR auto VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                          VkDebugUtilsMessageTypeFlagsEXT messageType,
                                          const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                          void* /*pUserData*/) -> VkBool32;

auto tr::renderer::Instance::init(const tr::Options& options, std::span<const char*> required_wsi_extensions)
    -> tr::renderer::Instance {
  Instance instance;

  if (options.debug.validations_layers) {  // validations_layers
    spdlog::trace("Available layers:");
    const std::set<std::string> available_layers_set = INLINE_LAMBDA {
      const std::vector<VkLayerProperties> available_layers = INLINE_LAMBDA {
        std::uint32_t layer_count = 0;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
        std::vector<VkLayerProperties> layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, layers.data());
        return layers;
      };

      std::set<std::string> layers_set;
      for (const auto& layer : available_layers) {
        spdlog::trace("\t{}", layer.layerName);
        layers_set.insert(layer.layerName);
      }
      return layers_set;
    };

    spdlog::trace("Wanted layers:");
    for (const auto& wanted_layer : OPTIONAL_VALIDATION_LAYERS) {
      spdlog::trace("\t{}", wanted_layer);
    }
    const std::set<std::string> wanted_set{OPTIONAL_VALIDATION_LAYERS.begin(), OPTIONAL_VALIDATION_LAYERS.end()};

    std::set_intersection(wanted_set.begin(), wanted_set.end(), available_layers_set.begin(),
                          available_layers_set.end(),
                          std::inserter(instance.validation_layers, instance.validation_layers.begin()));
    if (OPTIONAL_VALIDATION_LAYERS.size() != instance.validation_layers.size()) {
      spdlog::warn("{} wanted layers are satisfied out of {}", instance.validation_layers.size(),
                   OPTIONAL_VALIDATION_LAYERS.size());
    }
  }

  {  // Extensions
    std::vector<const char*> required_extensions{
        REQUIRED_INSTANCE_EXTENSIONS.begin(),
        REQUIRED_INSTANCE_EXTENSIONS.end(),
    };
    std::copy(required_wsi_extensions.begin(), required_wsi_extensions.end(), std::back_inserter(required_extensions));
    if (options.debug.validations_layers) {
      required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    const std::vector<VkExtensionProperties> available_extensions = INLINE_LAMBDA {
      std::uint32_t extension_count = 0;
      vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);

      std::vector<VkExtensionProperties> extensions_(extension_count);
      vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions_.data());
      return extensions_;
    };

    auto extensions =
        check_extensions("instance", required_extensions, OPTIONAL_INSTANCE_EXTENSIONS, available_extensions);
    TR_ASSERT(extensions.has_value(), "Required extensions are not satisfied");
    instance.extensions = *extensions;
  }

  {
    const VkDebugUtilsMessengerCreateInfoEXT create_info_debug_utils{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = nullptr,
        .flags = 0,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT,
        .pfnUserCallback = debug_callback,
        .pUserData = nullptr,
    };

    {
      const VkApplicationInfo appInfo{
          .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
          .pNext = nullptr,
          .pApplicationName = "ToyRenderer",
          .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
          .pEngineName = "ToyRenderer",
          .engineVersion = VK_MAKE_VERSION(1, 0, 0),
          .apiVersion = VK_API_VERSION_1_3,
      };

      const std::vector<const char*> extensions = INLINE_LAMBDA {
        const auto r = instance.extensions | std::views::transform(&std::string::c_str);
        return std::vector<const char*>{r.begin(), r.end()};
      };
      const std::vector<const char*> validation_layers = INLINE_LAMBDA {
        const auto r = instance.validation_layers | std::views::transform(&std::string::c_str);
        return std::vector<const char*>{r.begin(), r.end()};
      };

      const VkInstanceCreateInfo createInfo{
          .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
          .pNext = options.debug.validations_layers ? &create_info_debug_utils : nullptr,
          .flags = 0,
          .pApplicationInfo = &appInfo,
          .enabledLayerCount = utils::narrow_cast<std::uint32_t>(validation_layers.size()),
          .ppEnabledLayerNames = validation_layers.data(),
          .enabledExtensionCount = utils::narrow_cast<std::uint32_t>(extensions.size()),
          .ppEnabledExtensionNames = extensions.data(),
      };

      VK_UNWRAP(vkCreateInstance, &createInfo, nullptr, &instance.vk_instance);
      load_extensions(instance.vk_instance, EXTENSION_FLAG_DEBUG_UTILS);
    }

    if (options.debug.validations_layers) {
      VK_UNWRAP(vkCreateDebugUtilsMessengerEXT, instance.vk_instance, &create_info_debug_utils, nullptr,
                &instance.vk_debug_utils_messenger_ext);
    }
  }
  return instance;
}

VKAPI_ATTR auto VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
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

  std::string messageIdName = "no name";
  if (pCallbackData->pMessageIdName != nullptr) {
    messageIdName = pCallbackData->pMessageIdName;
  }
  std::string message = "no message";
  if (pCallbackData->pMessage != nullptr) {
    message = pCallbackData->pMessage;
  }
  spdlog::log(level, "[{}] - ({}:{}): {}", reason, messageIdName, pCallbackData->messageIdNumber,
              pCallbackData->pMessage);

  if (pCallbackData->objectCount > 0) {
    const char* objectName = "unnamed object";
    if (pCallbackData->pObjects[0].pObjectName != nullptr) {
      objectName = pCallbackData->pObjects[0].pObjectName;
    }

    spdlog::log(level,
                "[{}] - ({}:{}): 1st object affected {} "
                "(type: {}, name:\"{}\")",
                reason, messageIdName, pCallbackData->messageIdNumber, pCallbackData->pObjects[0].objectHandle,
                pCallbackData->pObjects[0].objectType, objectName);
  }

  return VK_FALSE;
}
