

#include "input.h"

#include <GLFW/glfw3.h>
#include <utils/cast.h>

void tr::system::Input::on_input(tr::system::InputEvent event) {
  switch (event.kind) {
    case InputEventKind::Key: {
      bool pressed = event.key_event.action == GLFW_RELEASE;
      switch (event.key_event.key) {
        case GLFW_KEY_W:
          state.camera_input.Forward = pressed;
          break;
        case GLFW_KEY_S:
          state.camera_input.Backward = pressed;
          break;
        case GLFW_KEY_A:
          state.camera_input.Left = pressed;
          break;
        case GLFW_KEY_D:
          state.camera_input.Right = pressed;
          break;
        case GLFW_KEY_UP:
          state.camera_input.RotUp = pressed;
          break;
        case GLFW_KEY_DOWN:
          state.camera_input.RotDown = pressed;
          break;
        case GLFW_KEY_LEFT:
          state.camera_input.RotLeft = pressed;
          break;
        case GLFW_KEY_RIGHT:
          state.camera_input.RotRight = pressed;
          break;
        default:
          break;
      }
      break;
    }
    case InputEventKind::CursorPos: {
      const auto old_cursor_pos = state.cursor_pos;
      state.cursor_pos.x = utils::narrow_cast<float>(event.cursor_pos_event.x);
      state.cursor_pos.y = utils::narrow_cast<float>(event.cursor_pos_event.x);

      state.camera_input.MouseDelta = state.cursor_pos - old_cursor_pos;
      break;
    }
    case InputEventKind::MouseButton:
      break;
  }
}

auto tr::system::Input::consume_camera_input() -> CameraInput {
  const auto camera_input = state.camera_input;
  state.camera_input.MouseDelta = {};
  return camera_input;
}
