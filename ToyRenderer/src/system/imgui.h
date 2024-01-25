#pragma once

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan_core.h>

#include "../renderer/vulkan_engine.h"

namespace tr::system {

class Imgui {
 public:
  void init(GLFWwindow *window, renderer::VulkanEngine &engine);

  [[nodiscard]] auto start_frame(renderer::Frame &frame) const -> bool;

  void draw(renderer::Frame &frame) const;

  bool valid = false;
  ~Imgui();

  Imgui() = default;
  Imgui(Imgui &&other) noexcept : valid(other.valid) { other.valid = false; };
  auto operator=(Imgui &&other) noexcept -> Imgui &;

  Imgui(const Imgui &) = delete;
  auto operator=(const Imgui &) -> Imgui & = delete;

 private:
  explicit Imgui(bool valid_) : valid(valid_) {}
};
}  // namespace tr::system
