#include "app.h"

#include <spdlog/spdlog.h>
#include <utils/assert.h>
#include <utils/misc.h>

#include <cstdint>
#include <vector>

#include "options.h"
#include "system/input.h"
#include "system/platform.h"
#include "utils/types.h"

void tr::App::update() {}

tr::App::App(tr::Options options) : options(options) {
  subsystems.platform.init(this);

  std::vector<const char*> required_vulkan_extensions;
  subsystems.platform.required_vulkan_extensions(required_vulkan_extensions);
  subsystems.engine.init(options, required_vulkan_extensions, subsystems.platform.window);
}

void tr::App::on_input(tr::system::InputEvent event) { subsystems.input.on_input(event); }

void tr::App::on_resize(utils::types::Extent2d<std::uint32_t> new_size) {
  utils::ignore_unused(this);
  utils::ignore_unused(new_size);
  /* TR_ASSERT(false, "NOT IMPLEMENTED ! New size: {}", new_size.width, */
  /*           new_size.height); */
}

void tr::App::run() {
  while (true) {
    state.frame_timer.start();
    subsystems.engine.start_frame();
    if (!subsystems.platform.start_frame()) {
      break;
    }

    subsystems.engine.draw();

    update();

    state.frame_timer.stop();
    state.timeline.push(state.frame_timer.elapsed_raw());

    auto elapsed = state.frame_timer.elapsed();
    spdlog::trace("Frame took {:.1f}us or {:.0f} FPS", elapsed * 1000, 1000. / elapsed);
    subsystems.engine.record_timeline();
  }
}
