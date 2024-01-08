#pragma once

#include <spdlog/spdlog.h>
#include <vulkan/vulkan_core.h>

namespace tr {

struct Options {
  struct {
    spdlog::level::level_enum level;
    bool renderdoc;
    bool validations_layers;
  } debug{};

  struct {
    VkPresentModeKHR prefered_present_mode = VK_PRESENT_MODE_FIFO_KHR;
  } config{};

  static auto from_args(std::span<const char *> args) -> Options;
};

}  // namespace tr
