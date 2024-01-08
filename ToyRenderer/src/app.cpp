#include "app.h"

#include <spdlog/spdlog.h>
#include <utils/assert.h>
#include <utils/misc.h>

#include <cstdint>
#include <vector>

#include "options.h"
#include "system/imgui.h"
#include "system/input.h"
#include "system/platform.h"
#include "utils/types.h"

void tr::App::update() {}

tr::App::App(tr::Options options) : options(options) {
  subsystems.platform.init(this);

  std::vector<const char*> required_vulkan_extensions;
  subsystems.platform.required_vulkan_extensions(required_vulkan_extensions);
  subsystems.engine.init(options, required_vulkan_extensions, subsystems.platform.window);

  subsystems.imgui = system::Imgui::init(subsystems.platform.window, subsystems.engine);
}

void tr::App::on_input(tr::system::InputEvent event) { subsystems.input.on_input(event); }
void tr::App::on_resize(utils::types::Extent2d<std::uint32_t> new_size) {
  utils::ignore_unused(new_size);
  subsystems.engine.on_resize();
}

void tr::App::run() {
  while (true) {
    state.frame_timer.start();
    if (!subsystems.platform.start_frame()) {
      break;
    }

    update();

    if (auto [ok, frame] = subsystems.engine.start_frame(); ok) {
      system::Imgui::start_frame();

      subsystems.engine.draw(frame);
      system::Imgui::draw(subsystems.engine.swapchain, frame);
      subsystems.engine.end_frame(frame);
    }

    state.frame_timer.stop();
    state.timeline.push(state.frame_timer.elapsed_raw());

    auto elapsed = state.frame_timer.elapsed();
    spdlog::trace("Frame took {:.1f}us or {:.0f} FPS", elapsed * 1000, 1000. / elapsed);
    subsystems.engine.record_timeline();
  }

  subsystems.engine.sync();
}
