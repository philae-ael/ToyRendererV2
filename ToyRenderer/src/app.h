#pragma once

#include <utils/timer.h>
#include <utils/types.h>

#include <cstdint>

#include "options.h"
#include "renderer/vulkan_engine.h"
#include "system/input.h"
#include "system/platform.h"

namespace tr {
class App {
 public:
  explicit App(Options options);
  void run();

  void on_resize(utils::types::Extent2d<std::uint32_t>);
  void on_input(system::InputEvent);

 private:
  void update();

  Options options;

  struct Subsystems {
    system::Platform platform;
    system::Input input;
    renderer::VulkanEngine engine;
  } subsystems;

  struct State {
    utils::FilteredTimer frame_timer;
    utils::Timeline<float> timeline;
  } state;
};
}  // namespace tr
