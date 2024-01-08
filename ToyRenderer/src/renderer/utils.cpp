

#include "utils.h"

#include <spdlog/spdlog.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <iterator>
#include <set>
#include <span>

auto tr::renderer::check_extensions(const char* kind, std::span<const char* const> required,
                                    std::span<VkExtensionProperties> available_extensions) -> bool {
  std::set<std::string> available_set;
  spdlog::trace("Available {} extensions", kind);
  for (const auto& extension : available_extensions) {
    spdlog::trace("\t{}", extension.extensionName);
    available_set.insert(extension.extensionName);
  }

  std::set<std::string> required_set{required.begin(), required.end()};
  spdlog::trace("Required {} extensions:", kind);
  for (const auto& required_extension : required) {
    spdlog::trace("\t{}", required_extension);
  }

  std::vector<std::string> out;
  std::set_difference(required_set.begin(), required_set.end(), available_set.begin(), available_set.end(),
                      std::back_inserter(out));

  spdlog::trace("Missing {} extensions:", kind);
  if (!out.empty()) {
    for (const auto& missing : out) {
      spdlog::trace("\t{}:", missing);
    }
    return false;
  }
  return true;
}

#define ENUM_CASE_OBJECT_TYPE(name) \
  case VK_OBJECT_TYPE_##name:       \
    return #name;

auto tr::renderer::vkObjectTypeName(VkObjectType type) -> const char* {
  switch (type) {
    ENUM_CASE_OBJECT_TYPE(UNKNOWN)
    ENUM_CASE_OBJECT_TYPE(INSTANCE)
    ENUM_CASE_OBJECT_TYPE(PHYSICAL_DEVICE)
    ENUM_CASE_OBJECT_TYPE(DEVICE)
    ENUM_CASE_OBJECT_TYPE(QUEUE)
    ENUM_CASE_OBJECT_TYPE(SEMAPHORE)
    ENUM_CASE_OBJECT_TYPE(COMMAND_BUFFER)
    ENUM_CASE_OBJECT_TYPE(FENCE)
    ENUM_CASE_OBJECT_TYPE(DEVICE_MEMORY)
    ENUM_CASE_OBJECT_TYPE(BUFFER)
    ENUM_CASE_OBJECT_TYPE(IMAGE)
    ENUM_CASE_OBJECT_TYPE(EVENT)
    ENUM_CASE_OBJECT_TYPE(QUERY_POOL)
    ENUM_CASE_OBJECT_TYPE(BUFFER_VIEW)
    ENUM_CASE_OBJECT_TYPE(IMAGE_VIEW)
    ENUM_CASE_OBJECT_TYPE(SHADER_MODULE)
    ENUM_CASE_OBJECT_TYPE(PIPELINE_CACHE)
    ENUM_CASE_OBJECT_TYPE(PIPELINE_LAYOUT)
    ENUM_CASE_OBJECT_TYPE(RENDER_PASS)
    ENUM_CASE_OBJECT_TYPE(PIPELINE)
    ENUM_CASE_OBJECT_TYPE(DESCRIPTOR_SET_LAYOUT)
    ENUM_CASE_OBJECT_TYPE(SAMPLER)
    ENUM_CASE_OBJECT_TYPE(DESCRIPTOR_POOL)
    ENUM_CASE_OBJECT_TYPE(DESCRIPTOR_SET)
    ENUM_CASE_OBJECT_TYPE(FRAMEBUFFER)
    ENUM_CASE_OBJECT_TYPE(COMMAND_POOL)
    ENUM_CASE_OBJECT_TYPE(SAMPLER_YCBCR_CONVERSION)
    ENUM_CASE_OBJECT_TYPE(DESCRIPTOR_UPDATE_TEMPLATE)
    ENUM_CASE_OBJECT_TYPE(PRIVATE_DATA_SLOT)
    ENUM_CASE_OBJECT_TYPE(SURFACE_KHR)
    ENUM_CASE_OBJECT_TYPE(SWAPCHAIN_KHR)
    ENUM_CASE_OBJECT_TYPE(DISPLAY_KHR)
    ENUM_CASE_OBJECT_TYPE(DISPLAY_MODE_KHR)
    ENUM_CASE_OBJECT_TYPE(DEBUG_REPORT_CALLBACK_EXT)
    ENUM_CASE_OBJECT_TYPE(VIDEO_SESSION_KHR)
    ENUM_CASE_OBJECT_TYPE(VIDEO_SESSION_PARAMETERS_KHR)
    ENUM_CASE_OBJECT_TYPE(CU_MODULE_NVX)
    ENUM_CASE_OBJECT_TYPE(CU_FUNCTION_NVX)
    ENUM_CASE_OBJECT_TYPE(DEBUG_UTILS_MESSENGER_EXT)
    ENUM_CASE_OBJECT_TYPE(ACCELERATION_STRUCTURE_KHR)
    ENUM_CASE_OBJECT_TYPE(VALIDATION_CACHE_EXT)
    ENUM_CASE_OBJECT_TYPE(ACCELERATION_STRUCTURE_NV)
    ENUM_CASE_OBJECT_TYPE(PERFORMANCE_CONFIGURATION_INTEL)
    ENUM_CASE_OBJECT_TYPE(DEFERRED_OPERATION_KHR)
    ENUM_CASE_OBJECT_TYPE(INDIRECT_COMMANDS_LAYOUT_NV)
    ENUM_CASE_OBJECT_TYPE(CUDA_MODULE_NV)
    ENUM_CASE_OBJECT_TYPE(CUDA_FUNCTION_NV)
    ENUM_CASE_OBJECT_TYPE(BUFFER_COLLECTION_FUCHSIA)
    ENUM_CASE_OBJECT_TYPE(MICROMAP_EXT)
    ENUM_CASE_OBJECT_TYPE(OPTICAL_FLOW_SESSION_NV)
    ENUM_CASE_OBJECT_TYPE(SHADER_EXT)
    default:
      return "unknown";
  }
}
#undef ENUM_CASE_OBJECT_TYPE
