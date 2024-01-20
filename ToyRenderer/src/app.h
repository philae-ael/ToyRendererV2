#pragma once

#include <utils/timer.h>
#include <utils/types.h>

#include <cstdint>
#include <vector>

#include "camera.h"
#include "options.h"
#include "renderer/mesh.h"
#include "renderer/render_graph.h"
#include "renderer/vulkan_engine.h"
#include "system/imgui.h"
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
    system::Imgui imgui;
  } subsystems;

  struct State {
    utils::FilteredTimer frame_timer;
    utils::Timeline<float> timeline;
    CameraController camera_controller;
  } state;

  std::vector<renderer::Mesh> meshes;
  std::vector<renderer::DirectionalLight> point_lights;

  renderer::RenderGraph rendergraph;
};
}  // namespace tr
