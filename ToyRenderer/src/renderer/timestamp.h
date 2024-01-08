#pragma once

#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <cstdint>

#include "deletion_queue.h"
#include "device.h"
#include "utils.h"
namespace tr::renderer {

template <const size_t FRAMES, const size_t QUERY_COUNT>
struct Timestamp {
  static auto init(Device &device) -> Timestamp {
    Timestamp t{};
    VkQueryPoolCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = FRAMES * QUERY_COUNT,
        .pipelineStatistics = 0,
    };

    VK_UNWRAP(vkCreateQueryPool, device.vk_device, &create_info, nullptr, &t.query_pool);
    vkResetQueryPool(device.vk_device, t.query_pool, 0, FRAMES * QUERY_COUNT);
    t.to_ms = device.device_properties.limits.timestampPeriod / 1000000.0F;
    return t;
  }
  void reinit(Device &device) { *this = init(device); }

  auto write_cmd_query(VkCommandBuffer cmd, VkPipelineStageFlagBits pipelineStage, std::size_t frame_id,
                       std::size_t index) {
    if (query_pool == VK_NULL_HANDLE) {
      return;
    }
    vkCmdWriteTimestamp(cmd, pipelineStage, query_pool, query_index(frame_id, index));
  }

  void defer_deletion(DeviceDeletionStack &device_deletion_stack) {
    device_deletion_stack.defer_deletion(DeviceHandle::QueryPool, query_pool);
  }

  void reset_queries(VkCommandBuffer cmd, std::size_t frame_id) {
    vkCmdResetQueryPool(cmd, query_pool, query_index(frame_id, 0), QUERY_COUNT);
  }

  auto get(VkDevice device, std::size_t frame_id) -> bool {
    VkResult result = vkGetQueryPoolResults(device, query_pool, query_index(frame_id, 0), QUERY_COUNT,
                                            (QUERY_COUNT + 1) * sizeof(uint64_t),
                                            raw_timestamps.data() + raw_timestamps_index(frame_id, 0), sizeof(uint64_t),
                                            // TODO: do a thing with availibility bit, maybe
                                            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
    switch (result) {
      case VkResult::VK_NOT_READY:
        return false;
      default:
        VK_CHECK(result, vkGetQueryPoolResults);
    }
    return raw_timestamps.at(raw_timestamps_index(frame_id, QUERY_COUNT)) != 0;
  }

  auto fetch_elsapsed(std::size_t frame_id, std::size_t from, std::size_t to) -> float {
    return static_cast<float>(raw_timestamps.at(raw_timestamps_index(frame_id, to)) -
                              raw_timestamps.at(raw_timestamps_index(frame_id, from))) *
           to_ms;
  }

 private:
  auto query_index(std::size_t frame_id, std::size_t index) -> std::size_t {
    return (frame_id % FRAMES) * QUERY_COUNT + index;
  }
  auto raw_timestamps_index(std::size_t frame_id, std::size_t index) -> std::size_t {
    return (frame_id % FRAMES) * (QUERY_COUNT + 1) + index;
  }

  float to_ms{};
  std::array<uint64_t, FRAMES *(QUERY_COUNT + 1)> raw_timestamps;
  VkQueryPool query_pool = VK_NULL_HANDLE;
};
}  // namespace tr::renderer
