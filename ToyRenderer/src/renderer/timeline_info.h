#pragma once

#include <array>
namespace tr::renderer {

enum GPUTimestampIndex {
  GPU_TIMESTAMP_INDEX_TOP,
  GPU_TIMESTAMP_INDEX_BOTTOM,
  GPU_TIMESTAMP_INDEX_MAX,
};

enum CPUTimestampIndex {
  CPU_TIMESTAMP_INDEX_TOP,
  CPU_TIMESTAMP_INDEX_BOTTOM,
  CPU_TIMESTAMP_INDEX_PRESENT_END,
  CPU_TIMESTAMP_INDEX_MAX,
};

struct GPUTimePeriodDescriptor {
  const char* name;

  GPUTimestampIndex from;
  GPUTimestampIndex to;
};

struct CPUTimePeriodDescriptor {
  const char* name;

  CPUTimestampIndex from;
  CPUTimestampIndex to;
};

static const std::array<GPUTimePeriodDescriptor, 1> GPU_TIME_PERIODS{{
    {"ALL", GPU_TIMESTAMP_INDEX_TOP, GPU_TIMESTAMP_INDEX_BOTTOM},
}};

static const std::array<CPUTimePeriodDescriptor, 2> CPU_TIME_PERIODS{{
    {"Command buffer", CPU_TIMESTAMP_INDEX_TOP, CPU_TIMESTAMP_INDEX_BOTTOM},
    {"Present", CPU_TIMESTAMP_INDEX_BOTTOM, CPU_TIMESTAMP_INDEX_PRESENT_END},
}};

}  // namespace tr::renderer
