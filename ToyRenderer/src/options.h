#pragma once

#include <vulkan/vulkan_core.h>

#include <span>
#include <string_view>

namespace spdlog::level {
enum level_enum : int;
}

namespace tr {

struct Options {
  struct {
    spdlog::level::level_enum level;
    bool renderdoc = false;
    bool validations_layers = false;
    bool imgui = true;
  } debug{};

  struct {
    VkPresentModeKHR prefered_present_mode = VK_PRESENT_MODE_FIFO_KHR;
  } config{};

  std::string_view scene;

  static auto from_args(std::span<const char *> args) -> Options;
};

}  // namespace tr
