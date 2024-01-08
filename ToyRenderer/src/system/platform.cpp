#include "platform.h"

#include <GLFW/glfw3.h>
#include <utils/assert.h>

#include <cstdint>
#include <vector>

#include "../app.h"

const auto WinWidthInitial = 1920;
const auto WinHeightInitial = 1080;

auto glfw_extract_app(GLFWwindow* window) -> tr::App* {
  return reinterpret_cast<tr::App*>(glfwGetWindowUserPointer(window));
}

void resize_callback(GLFWwindow* window, int width, int height) {
  glfw_extract_app(window)->on_resize({
      static_cast<std::uint32_t>(width),
      static_cast<std::uint32_t>(height),
  });
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  glfw_extract_app(window)->on_input({
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
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
  glfw_extract_app(window)->on_input({
      .kind = tr::system::InputEventKind::MouseButton,
      .mouse_button_event =
          {
              .button = button,
              .action = action,
              .mods = mods,
          },
  });
}

void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
  glfw_extract_app(window)->on_input({
      .kind = tr::system::InputEventKind::CursorPos,
      .cursor_pos_event =
          {
              .x = xpos,
              .y = ypos,
          },
  });
}

void tr::system::Platform::init(tr::App* app) {
  TR_ASSERT(glfwInit() != 0, "could not initializa GLFW");

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  window = glfwCreateWindow(WinWidthInitial, WinHeightInitial, "Toy Renderer", nullptr, nullptr);
  TR_ASSERT(window, "could not open window");

  glfwSetWindowUserPointer(window, reinterpret_cast<void*>(app));
  glfwSetWindowSizeCallback(window, resize_callback);
  glfwSetKeyCallback(window, key_callback);
  glfwSetMouseButtonCallback(window, mouse_button_callback);
  glfwSetCursorPosCallback(window, cursor_pos_callback);
}

void tr::system::Platform::required_vulkan_extensions(std::vector<const char*>& extensions) {
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
  return glfwWindowShouldClose(window) == 0;
}

void tr::system::Platform::destroy() {
  if (window != nullptr) {
    glfwDestroyWindow(window);
    glfwTerminate();
    window = nullptr;
  }
}
