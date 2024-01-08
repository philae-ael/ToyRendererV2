#pragma once

#include <spdlog/common.h>
#include <vulkan/vulkan_core.h>

namespace tr {

struct Options {
  struct {
    spdlog::level::level_enum level = spdlog::level::info;
    bool renderdoc = false;
    bool validations_layers = false;
    bool imgui = true;
  } debug{};

  struct {
    VkPresentModeKHR prefered_present_mode = VK_PRESENT_MODE_FIFO_KHR;
  } config{};

  static auto from_args(std::span<const char *> args) -> Options;
};

}  // namespace tr
