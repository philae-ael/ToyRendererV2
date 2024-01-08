#pragma once

#include <glm/vec2.hpp>

#include "../camera.h"

namespace tr::system {

enum class InputEventKind {
  Key,
  CursorPos,
  MouseButton,
};

struct InputEvent {
  InputEventKind kind;
  union {
    struct {
      int key;
      int scancode;
      int action;
      int mods;
    } key_event;
    struct {
      double x;
      double y;
    } cursor_pos_event;
    struct {
      int button;
      int action;
      int mods;
    } mouse_button_event;
  };
};

class Input {
 public:
  void on_input(InputEvent);
  [[nodiscard]] auto consume_camera_input() -> CameraInput;

  struct State {
    glm::vec2 cursor_pos{};
    CameraInput camera_input;
  } state;
};
}  // namespace tr::system
