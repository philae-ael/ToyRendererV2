#pragma once

#include <imgui.h>
#include <spdlog/spdlog.h>
#include <utils/assert.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <ratio>

#include "debug.h"
#include "deletion_queue.h"
#include "device.h"
#include "instance.h"
#include "pipeline.h"
#include "renderpass.h"
#include "surface.h"
#include "swapchain.h"
#include "timeline_info.h"
#include "timestamp.h"
#include "utils.h"
#include "utils/math.h"
#include "utils/timer.h"
#include "vertex.h"

namespace tr::system {
class Imgui;
}

namespace tr::renderer {

const std::size_t MAX_IN_FLIGHT = 1;

class VulkanEngine {
 public:
  VulkanEngine() = default;
  void init(tr::Options& options, std::span<const char*> required_instance_extensions, GLFWwindow* window);

  void on_resize() { fb_resized = true; }

  auto start_frame() -> std::pair<bool, Frame>;
  void draw(Frame);
  void end_frame(Frame);

  void record_timeline();

  ~VulkanEngine();

  VulkanEngine(const VulkanEngine&) = delete;
  VulkanEngine(VulkanEngine&&) = delete;
  auto operator=(const VulkanEngine&) -> VulkanEngine& = delete;
  auto operator=(VulkanEngine&&) -> VulkanEngine& = delete;

  void sync() const { VK_UNWRAP(vkDeviceWaitIdle, device.vk_device); }
  void rebuild_swapchain();

  void imgui();

 private:
  friend system::Imgui;

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
  bool fb_resized = false;

  std::array<FrameSynchro, MAX_IN_FLIGHT> frame_synchronisation_pool;

  // Buffer should be moved i guess
  VkCommandPool graphics_command_pool = VK_NULL_HANDLE;
  std::array<VkCommandBuffer, MAX_IN_FLIGHT> main_command_buffer_pool{};

  VkCommandPool transfer_command_pool = VK_NULL_HANDLE;
  VkCommandBuffer transfer_command_buffer{};

  Buffer triangle_vertex_buffer;
  StagingBuffer staging_buffer;

  Pipeline pipeline;

  // DEBUG AND TIMING
  void write_gpu_timestamp(VkCommandBuffer cmd, VkPipelineStageFlagBits pipelineStage, GPUTimestampIndex index);
  void write_cpu_timestamp(tr::renderer::CPUTimestampIndex index);

  Timestamp<MAX_IN_FLIGHT, GPU_TIMESTAMP_INDEX_MAX> gpu_timestamps;
  using cpu_timestamp_clock = std::chrono::high_resolution_clock;
  std::array<cpu_timestamp_clock::time_point, CPU_TIMESTAMP_INDEX_MAX> cpu_timestamps;

  std::array<utils::Timeline<float, 500>, GPU_TIME_PERIODS.size()> gpu_timelines{};
  std::array<utils::math::KalmanFilter<float>, GPU_TIME_PERIODS.size()> avg_gpu_timelines{};

  std::array<utils::Timeline<float, 500>, CPU_TIME_PERIODS.size()> cpu_timelines{};
  std::array<utils::math::KalmanFilter<float>, CPU_TIME_PERIODS.size()> avg_cpu_timelines{};

  std::size_t frame_id{};
};

}  // namespace tr::renderer
