#pragma once

#include <vector>

#include "utils/types.h"
struct GLFWwindow;
namespace tr {
class App;
}

namespace tr::system {
class Platform {
 public:
  Platform() = default;
  auto init(tr::App *)->utils::types::Extent2d<int>;

  ~Platform() { destroy(); }
  void destroy();

  void required_vulkan_extensions(std::vector<const char *> &);

  auto start_frame() -> bool;

  void on_resize(int width, int height);
  void on_key(int key, int scancode, int action, int mods);
  void on_mouse_move(double xpos, double ypos);
  void on_mouse_button(int button, int action, int mods);

  Platform(Platform &&other) = delete;
  auto operator=(Platform &&other) = delete;
  Platform(const Platform &) = delete;
  auto operator=(const Platform &) -> Platform & = delete;

  GLFWwindow *window = nullptr;

 private:
  bool minimized = false;
  App *app = nullptr;
};
}  // namespace tr::system
