#include "platform.h"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#include <utils/assert.h>

#include <cstdint>
#include <vector>

#include "../app.h"
#include "../registry.h"
#include "utils/misc.h"

const auto WinWidthInitial = 1080;
const auto WinHeightInitial = 720;

auto glfw_extract_platform(GLFWwindow* window) -> tr::system::Platform* {
  return reinterpret_cast<tr::system::Platform*>(glfwGetWindowUserPointer(window));
}

void resize_callback(GLFWwindow* window, int width, int height) {
  glfw_extract_platform(window)->on_resize(width, height);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  glfw_extract_platform(window)->on_key(key, scancode, action, mods);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
  glfw_extract_platform(window)->on_mouse_button(button, action, mods);
}

void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
  glfw_extract_platform(window)->on_mouse_move(xpos, ypos);
}

void tr::system::Platform::init(tr::App* new_app) {
  TR_ASSERT(glfwInit() != 0, "could not initializa GLFW");

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  this->app = new_app;

  auto width = Registry::global()["screen"].get("width", WinWidthInitial).asInt();
  auto height = Registry::global()["screen"].get("height", WinHeightInitial).asInt();

  window = glfwCreateWindow(width, height, "Toy Renderer", nullptr, nullptr);
  TR_ASSERT(window, "could not open window");

  glfwSetWindowUserPointer(window, reinterpret_cast<void*>(this));
  glfwSetWindowSizeCallback(window, resize_callback);
  glfwSetKeyCallback(window, key_callback);
  glfwSetMouseButtonCallback(window, mouse_button_callback);
  glfwSetCursorPosCallback(window, cursor_pos_callback);
}

void tr::system::Platform::required_vulkan_extensions(std::vector<const char*>& extensions) {
  utils::ignore_unused(this);
  uint32_t glfw_extensions_count = 0;
  const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extensions_count);

  TR_ASSERT(glfw_extensions != nullptr,
            "Can't get required instances extensions from gltfw; is vulkan "
            "supported?");

  extensions.reserve(extensions.size() + glfw_extensions_count);

  for (const char* extension : std::span(glfw_extensions, glfw_extensions_count)) {
    extensions.push_back(extension);
  }
}

auto tr::system::Platform::start_frame() -> bool {
  glfwPollEvents();

  if (minimized) {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
      glfwGetFramebufferSize(window, &width, &height);
      glfwWaitEvents();
    }
    minimized = false;
  }
  return glfwWindowShouldClose(window) == 0;
}

void tr::system::Platform::destroy() {
  if (window != nullptr) {
    glfwDestroyWindow(window);
    glfwTerminate();
    window = nullptr;
  }
}

void tr::system::Platform::on_resize(int width, int height) {
  minimized = width == 0 || height == 0;
  app->on_resize({
      static_cast<std::uint32_t>(width),
      static_cast<std::uint32_t>(height),
  });
}

void tr::system::Platform::on_key(int key, int scancode, int action, int mods) {
  app->on_input({
      .kind = tr::system::InputEventKind::Key,
      .key_event =
          {
              .key = key,
              .scancode = scancode,
              .action = action,
              .mods = mods,
          },
  });
}
void tr::system::Platform::on_mouse_move(double xpos, double ypos) {
  app->on_input({
      .kind = tr::system::InputEventKind::CursorPos,
      .cursor_pos_event =
          {
              .x = xpos,
              .y = ypos,
          },
  });
}
void tr::system::Platform::on_mouse_button(int button, int action, int mods) {
  app->on_input({
      .kind = tr::system::InputEventKind::MouseButton,
      .mouse_button_event =
          {
              .button = button,
              .action = action,
              .mods = mods,
          },
  });
}
