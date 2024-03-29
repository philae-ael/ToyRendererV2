#include "app.h"

#include <spdlog/spdlog.h>
#include <utils/misc.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "camera.h"
#include "gltf.h"
#include "options.h"
#include "renderer/debug.h"
#include "renderer/render_graph.h"
#include "system/imgui.h"
#include "system/input.h"
#include "system/platform.h"
#include "utils/timer.h"
#include "utils/types.h"

namespace tr::renderer {
struct Frame;
struct Transferer;
}  // namespace tr::renderer

void tr::App::update() {
  const auto dt = state.frame_timer.elapsed();  // millis
  state.camera_controller.update(subsystems.input.consume_camera_input(), dt / 1000);
}

tr::App::App(tr::Options options_) : options(options_) {
  auto win_size = subsystems.platform.init(this);
  state.camera_controller.camera.aspectRatio = win_size.aspect_ratio();

  std::vector<const char *> required_vulkan_extensions;
  subsystems.platform.required_vulkan_extensions(required_vulkan_extensions);

  TIMED_INLINE_LAMBDA("Load engine") {
    subsystems.engine.init(options, required_vulkan_extensions, subsystems.platform.window);
  };

  if (options.debug.imgui) {
    subsystems.imgui.init(subsystems.platform.window, subsystems.engine);
  }

  rendergraph = std::make_unique<renderer::RenderGraph>();
  subsystems.engine.transfer([&](renderer::Transferer &t) {
    rendergraph->init(subsystems.engine, t);

    TIMED_INLINE_LAMBDA("Load scene") {
      std::string scene_name;
      if (options.scene.empty()) {
        scene_name = "assets/scenes/sponza/Sponza.gltf";
      } else {
        scene_name = options.scene;
      }
      auto bb = subsystems.engine.buffer_builder();
      auto ib = subsystems.engine.image_builder();
      const auto [_, scene] =
          Gltf::load_from_file(subsystems.engine.lifetime.global, ib, bb, t, subsystems.engine.rm, scene_name);
      meshes.insert(meshes.end(), scene.begin(), scene.end());

      std::size_t surface_count = 0;
      for (const auto &mesh : meshes) {
        surface_count += mesh.surfaces.size();
      }
      spdlog::info("There are {} meshes and {} surfaces ", meshes.size(), surface_count);
    };
  });
  subsystems.engine.sync();
}

void tr::App::on_input(tr::system::InputEvent event) { subsystems.input.on_input(event); }
void tr::App::on_resize(utils::types::Extent2d<std::uint32_t> new_size) {
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

    subsystems.engine.frame([&](renderer::Frame &frame) {
      rendergraph->draw(frame, meshes, state.camera_controller.camera);

      if (subsystems.imgui.start_frame(frame)) {
        subsystems.engine.imgui();
        rendergraph->imgui(subsystems.engine);
        subsystems.imgui.draw(frame);
      }
    });

    state.frame_timer.stop();
    state.timeline.push(state.frame_timer.elapsed_raw());

    auto elapsed = state.frame_timer.elapsed();
    spdlog::trace("Frame took {:.1f}us or {:.0f} FPS", elapsed * 1000, 1000. / elapsed);
    subsystems.engine.debug_info.record_timeline(subsystems.engine);
  }

  subsystems.engine.sync();
}
tr::App::~App() = default;
