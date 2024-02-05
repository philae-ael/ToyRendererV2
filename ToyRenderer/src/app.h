#pragma once

#include <utils/timer.h>

#include <cstdint>
#include <vector>

#include "camera.h"
#include "options.h"
#include "renderer/mesh.h"
#include "renderer/vulkan_engine.h"
#include "system/imgui.h"
#include "system/input.h"
#include "system/platform.h"

namespace utils::types {
template <class T>
struct Extent2d;
}  // namespace utils::types

namespace tr {
namespace renderer {
class RenderGraph;
}
class App {
 public:
  App(const App &) = delete;
  App(App &&) = delete;
  auto operator=(const App &) -> App & = delete;
  auto operator=(App &&) -> App & = delete;
  explicit App(Options options);
  ~App();

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

  std::unique_ptr<renderer::RenderGraph> rendergraph;
};
}  // namespace tr
