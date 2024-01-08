#pragma once

#include <vector>
struct GLFWwindow;
namespace tr {
class App;
}

namespace tr::system {
class Platform {
 public:
  // init
  Platform() = default;
  void init(tr::App *);

  void required_vulkan_extensions(std::vector<const char *> &);

  // destroy
  void destroy();
  ~Platform() { destroy(); }

  // normal use
  auto start_frame() -> bool;

  // move
  Platform(Platform &&other) noexcept : window(other.window) { other.window = nullptr; }
  auto operator=(Platform &&other) noexcept -> Platform & {
    destroy();
    window = other.window;
    other.window = nullptr;
    return *this;
  }

  // no copy!
  Platform(const Platform &) = delete;
  auto operator=(const Platform &) -> Platform & = delete;

  GLFWwindow *window = nullptr;
};
}  // namespace tr::system
