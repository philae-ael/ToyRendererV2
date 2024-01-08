#pragma once

#include <spdlog/spdlog.h>
#include <utils/assert.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <ratio>

#include "debug.h"
#include "deletion_queue.h"
#include "device.h"
#include "instance.h"
#include "renderpass.h"
#include "surface.h"
#include "swapchain.h"
#include "timeline_info.h"
#include "timestamp.h"
#include "utils.h"
#include "utils/math.h"
#include "utils/timer.h"

namespace tr::renderer {
class VulkanEngine {
 public:
  VulkanEngine() = default;
  void init(tr::Options& options, std::span<const char*> required_instance_extensions, GLFWwindow* window);

  void sync() const {
    VK_UNWRAP(vkQueueWaitIdle, device.queues.graphics_queue);
    VK_UNWRAP(vkQueueWaitIdle, device.queues.present_queue);
  }

  void rebuild_swapchain();

  void start_frame() { frame_id += 1; }
  void draw();

  void record_timeline() {
    if (gpu_timestamps.get(device.vk_device, frame_id - 1)) {
      for (std::size_t i = 0; i < GPU_TIME_PERIODS.size(); i++) {
        auto& period = GPU_TIME_PERIODS[i];
        auto dt = gpu_timestamps.fetch_elsapsed(frame_id - 1, period.from, period.to);
        avg_gpu_timelines[i].update(dt);
        gpu_timelines[i].push(dt);

        spdlog::trace("GPU Took {:.3f}us (smoothed {:3f}us) for period {}", 1000. * dt,
                      1000. * avg_gpu_timelines[i].state, period.name);
      }
    }

    for (std::size_t i = 0; i < CPU_TIME_PERIODS.size(); i++) {
      auto& period = CPU_TIME_PERIODS[i];
      float dt =
          std::chrono::duration<float, std::milli>(cpu_timestamps[period.to] - cpu_timestamps[period.from]).count();
      avg_cpu_timelines[i].update(dt);
      cpu_timelines[i].push(dt);

      spdlog::trace("CPU Took {:.3f}us (smoothed {:3f}us) for period {}", 1000. * dt,
                    1000. * avg_cpu_timelines[i].state, period.name);
    }
  }

  ~VulkanEngine();

  VulkanEngine(const VulkanEngine&) = delete;
  VulkanEngine(VulkanEngine&&) = delete;
  auto operator=(const VulkanEngine&) -> VulkanEngine& = delete;
  auto operator=(VulkanEngine&&) -> VulkanEngine& = delete;

 private:
  GLFWwindow* window{};

  // Cleaned at exit
  struct DeletionStacks {
    InstanceDeletionStack instance;
    DeviceDeletionStack device;
  } global_deletion_stacks;

  // Cleaned on swapchain recreation
  DeviceDeletionStack swapchain_device_deletion_stack;

  // Cleaned every frame
  DeviceDeletionStack frame_device_deletion_stack;

  Renderdoc renderdoc;
  Instance instance;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  Device device;

  // Swapchain relataed
  Renderpass renderpass;
  Swapchain swapchain;

  FrameSynchroPool frame_synchronisation_pool;

  // Buffer should be moved i guess
  VkCommandPool graphics_command_pool = VK_NULL_HANDLE;
  VkCommandBuffer main_command_buffer = VK_NULL_HANDLE;

  // DEBUG AND TIMING
  void write_gpu_timestamp(VkCommandBuffer cmd, VkPipelineStageFlagBits pipelineStage, GPUTimestampIndex index);
  void write_cpu_timestamp(tr::renderer::CPUTimestampIndex index);

  Timestamp<2, GPU_TIMESTAMP_INDEX_MAX> gpu_timestamps;
  using cpu_timestamp_clock = std::chrono::high_resolution_clock;
  std::array<cpu_timestamp_clock::time_point, CPU_TIMESTAMP_INDEX_MAX> cpu_timestamps;

  std::array<utils::Timeline<float>, GPU_TIME_PERIODS.size()> gpu_timelines{};
  std::array<utils::math::KalmanFilter<float>, GPU_TIME_PERIODS.size()> avg_gpu_timelines{};

  std::array<utils::Timeline<float>, CPU_TIME_PERIODS.size()> cpu_timelines{};
  std::array<utils::math::KalmanFilter<float>, CPU_TIME_PERIODS.size()> avg_cpu_timelines{};

  std::size_t frame_id{};
};

}  // namespace tr::renderer
