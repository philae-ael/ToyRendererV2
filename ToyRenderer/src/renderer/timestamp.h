#pragma once

#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <cstdint>

#include "deletion_queue.h"
#include "device.h"
#include "utils.h"
namespace tr::renderer {

template <const size_t FRAMES, const size_t POINTS>
struct Timestamp {
  static auto init(Device &device) -> Timestamp {
    Timestamp t{};
    VkQueryPoolCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = t.raw_timestamps.size(),
        .pipelineStatistics = 0,
    };

    VK_UNWRAP(vkCreateQueryPool, device.vk_device, &create_info, nullptr,
              &t.query_pool);
    t.to_ms = device.device_properties.limits.timestampPeriod / 1000000.0F;
    return t;
  }

  auto write_cmd_query(VkCommandBuffer cmd,
                       VkPipelineStageFlagBits pipelineStage, uint32_t index) {
    if (query_pool == VK_NULL_HANDLE) {
      return;
    }
    vkCmdWriteTimestamp(cmd, pipelineStage, query_pool, index);
  }

  void defer_deletion(DeviceDeletionStack &device_deletion_stack) {
    device_deletion_stack.defer_deletion(DeviceHandle::QueryPool, query_pool);
  }

  void reset(VkDevice device) {
    vkResetQueryPool(device, query_pool, 0, raw_timestamps.size());
  }

  void get(VkDevice device) {
    VK_UNWRAP(vkGetQueryPoolResults, device, query_pool, 0,
              raw_timestamps.size(), raw_timestamps.size() * sizeof(uint64_t),
              raw_timestamps.data(), sizeof(uint64_t),
              VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
  }

  auto fetch_timestamp(uint32_t index) -> float {
    if (index >= raw_timestamps.size()) {
      return 0.0;
    }

    return static_cast<float>(raw_timestamps.at(index)) * to_ms;
  }

 private:
  float to_ms{};
  std::array<uint64_t, FRAMES * POINTS> raw_timestamps;
  VkQueryPool query_pool = VK_NULL_HANDLE;
};
}  // namespace tr::renderer
