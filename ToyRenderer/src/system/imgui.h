#pragma once

#include <cstdint>

namespace tr::renderer {
class VulkanEngine;
enum class image_ressource_handle : uint32_t;
struct Frame;
}  // namespace tr::renderer

struct GLFWwindow;

namespace tr::system {

class Imgui {
 public:
  renderer::image_ressource_handle swapchain_handle{};
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
