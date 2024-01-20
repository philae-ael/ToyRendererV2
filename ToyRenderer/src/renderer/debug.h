#pragma once
#include <renderdoc.h>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan_core.h>

#include <cstddef>

#include "constants.h"
#include "timeline_info.h"
#include "timestamp.h"
#include "utils/timer.h"

namespace tr::renderer {

class DebugCmdScope {
 public:
  explicit DebugCmdScope(VkCommandBuffer cmd_, const char *label) : cmd(cmd_) {
    const VkDebugUtilsLabelEXT label_info{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pNext = nullptr,
        .pLabelName = label,
        .color = {0.0, 0.0, 0.0, 1.0},
    };
    vkCmdBeginDebugUtilsLabelEXT(cmd_, &label_info);
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
  explicit DebugQueueScope(VkQueue queue_, const char *label) : queue(queue_) {
    const VkDebugUtilsLabelEXT label_info{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pNext = nullptr,
        .pLabelName = label,
        .color = {0.0, 0.0, 0.0, 1.0},
    };
    vkQueueBeginDebugUtilsLabelEXT(queue_, &label_info);
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
void set_debug_object_name(VkDevice device, VkObjectType type, T t, const std::string &name) {
  VkDebugUtilsObjectNameInfoEXT name_info{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .pNext = nullptr,
      .objectType = type,
      .objectHandle = reinterpret_cast<uint64_t>(t),
      .pObjectName = name.c_str(),
  };
  vkSetDebugUtilsObjectNameEXT(device, &name_info);
}

class VulkanEngine;

struct VulkanEngineDebugInfo {
  void set_frame_id(VkCommandBuffer cmd, std::size_t frame_id);
  void write_gpu_timestamp(VkCommandBuffer cmd, VkPipelineStageFlagBits pipelineStage, GPUTimestampIndex index);
  void write_cpu_timestamp(tr::renderer::CPUTimestampIndex index);
  void record_timeline(VulkanEngine &);

  void imgui(VulkanEngine &);

  std::size_t current_frame_id{};

  GPUTimestamp<MAX_FRAMES_IN_FLIGHT, GPU_TIMESTAMP_INDEX_MAX> gpu_timestamps;
  using cpu_timestamp_clock = std::chrono::high_resolution_clock;
  std::array<cpu_timestamp_clock::time_point, CPU_TIMESTAMP_INDEX_MAX> cpu_timestamps;

  std::array<utils::Timeline<float, 500>, GPU_TIME_PERIODS.size()> gpu_timelines{};
  std::array<utils::math::KalmanFilter<float>, GPU_TIME_PERIODS.size()> avg_gpu_timelines{};

  std::array<utils::Timeline<float, 500>, CPU_TIME_PERIODS.size()> cpu_timelines{};
  std::array<utils::math::KalmanFilter<float>, CPU_TIME_PERIODS.size()> avg_cpu_timelines{};

  std::array<utils::Timeline<float, 500>, VK_MAX_MEMORY_HEAPS> gpu_heaps_usage{};
  utils::Timeline<float, 500> gpu_memory_usage{};

  Renderdoc renderdoc;

 private:
  void stat_window(VulkanEngine &);
  void timings_info();
  void memory_info(VulkanEngine &);
  void option_window(VulkanEngine &);
};

}  // namespace tr::renderer
