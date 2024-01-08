

#include "input.h"

#include <utils/cast.h>

void tr::system::Input::on_input(tr::system::InputEvent event) {
  switch (event.kind) {
    case InputEventKind::Key:
      break;
    case InputEventKind::CursorPos:
      state.cursor_pos.x = utils::narrow_cast<float>(event.cursor_pos_event.x);
      state.cursor_pos.y = utils::narrow_cast<float>(event.cursor_pos_event.x);
      break;
    case InputEventKind::MouseButton:
      break;
  }
}
