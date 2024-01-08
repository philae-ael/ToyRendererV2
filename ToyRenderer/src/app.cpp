#include "app.h"

#include <spdlog/spdlog.h>
#include <utils/assert.h>
#include <utils/misc.h>

#include <cstdint>
#include <vector>

#include "camera.h"
#include "options.h"
#include "system/imgui.h"
#include "system/input.h"
#include "system/platform.h"
#include "utils/types.h"

void tr::App::update() {
  const auto dt = state.frame_timer.elapsed();  // millis
  state.camera_controller.update(subsystems.input.consume_camera_input(), dt / 1000);
  subsystems.engine.matrices = state.camera_controller.camera.cameraMatrices();
}

tr::App::App(tr::Options options) : options(options) {
  auto win_size = subsystems.platform.init(this);
  state.camera_controller.camera.aspectRatio = win_size.aspect_ratio();

  std::vector<const char*> required_vulkan_extensions;
  subsystems.platform.required_vulkan_extensions(required_vulkan_extensions);
  subsystems.engine.init(options, required_vulkan_extensions, subsystems.platform.window);

  if (options.debug.imgui) {
    subsystems.imgui.init(subsystems.platform.window, subsystems.engine);
  }
}

void tr::App::on_input(tr::system::InputEvent event) { subsystems.input.on_input(event); }
void tr::App::on_resize(utils::types::Extent2d<std::uint32_t> new_size) {
  utils::ignore_unused(new_size);
  subsystems.engine.on_resize();
  state.camera_controller.camera.aspectRatio = new_size.aspect_ratio();
}

void tr::App::run() {
  while (true) {
    state.frame_timer.start();
    if (!subsystems.platform.start_frame()) {
      break;
    }

    update();

    if (auto frame_opt = subsystems.engine.start_frame(); frame_opt.has_value()) {
      auto frame = frame_opt.value();
      subsystems.engine.draw(frame);
      if (subsystems.imgui.start_frame()) {
        subsystems.engine.debug_info.imgui(subsystems.engine);
        subsystems.imgui.draw(subsystems.engine, frame);
      }
      subsystems.engine.end_frame(frame);
    }

    state.frame_timer.stop();
    state.timeline.push(state.frame_timer.elapsed_raw());

    auto elapsed = state.frame_timer.elapsed();
    spdlog::trace("Frame took {:.1f}us or {:.0f} FPS", elapsed * 1000, 1000. / elapsed);
    subsystems.engine.debug_info.record_timeline(subsystems.engine);
  }

  subsystems.engine.sync();
}
