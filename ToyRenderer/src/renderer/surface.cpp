#include "surface.h"

#include <GLFW/glfw3.h>

#include "utils.h"

auto tr::renderer::Surface::init(VkInstance instance, GLFWwindow* window) -> VkSurfaceKHR {
  VkSurfaceKHR surface = VK_NULL_HANDLE;

  VK_UNWRAP(glfwCreateWindowSurface, instance, window, nullptr, &surface);

  return surface;
}
