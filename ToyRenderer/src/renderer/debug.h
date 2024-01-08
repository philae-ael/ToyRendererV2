#pragma once
#include <renderdoc.h>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan_core.h>

#include "utils.h"
namespace tr::renderer {

class DebugCmdScope {
 public:
  explicit DebugCmdScope(VkCommandBuffer cmd, const char *label) : cmd(cmd) {
    const VkDebugUtilsLabelEXT label_info{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pNext = nullptr,
        .pLabelName = label,
        .color = {0.0, 0.0, 0.0, 1.0},
    };
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label_info);
  }
  ~DebugCmdScope() { vkCmdEndDebugUtilsLabelEXT(cmd); }
  VkCommandBuffer cmd;

  DebugCmdScope(const DebugCmdScope &) = delete;
  DebugCmdScope(DebugCmdScope &&) = delete;
  auto operator=(const DebugCmdScope &) -> DebugCmdScope & = delete;
  auto operator=(DebugCmdScope &&) -> DebugCmdScope & = delete;
};

class DebugQueueScope {
 public:
  explicit DebugQueueScope(VkQueue queue, const char *label) : queue(queue) {
    const VkDebugUtilsLabelEXT label_info{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pNext = nullptr,
        .pLabelName = label,
        .color = {0.0, 0.0, 0.0, 1.0},
    };
    vkQueueBeginDebugUtilsLabelEXT(queue, &label_info);
  }
  ~DebugQueueScope() { vkQueueEndDebugUtilsLabelEXT(queue); }
  VkQueue queue;

  DebugQueueScope(const DebugQueueScope &) = delete;
  DebugQueueScope(DebugQueueScope &&) = delete;
  auto operator=(const DebugQueueScope &) -> DebugQueueScope & = delete;
  auto operator=(DebugQueueScope &&) -> DebugQueueScope & = delete;
};

struct Renderdoc {
  static auto init() -> Renderdoc;
  void TriggerCapture() const {
    if (rdoc_api != nullptr) {
      rdoc_api->TriggerCapture();
      spdlog::info("next frame will be captured");
    }
  }

  RENDERDOC_API_1_1_2 *rdoc_api = nullptr;
};

template <class T>
void set_debug_object_name(VkDevice device, VkObjectType type, T t, const char *name) {
  VkDebugUtilsObjectNameInfoEXT name_info{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .pNext = nullptr,
      .objectType = type,
      .objectHandle = reinterpret_cast<uint64_t>(t),
      .pObjectName = name,
  };
  vkSetDebugUtilsObjectNameEXT(device, &name_info);
}

}  // namespace tr::renderer
