#include "app.h"

#include <spdlog/spdlog.h>
#include <utils/assert.h>
#include <utils/misc.h>
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <utility>
#include <vector>

#include "camera.h"
#include "gltf.h"
#include "options.h"
#include "renderer/mesh.h"
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

  {
    auto t = subsystems.engine.start_transfer();
    auto bb = subsystems.engine.buffer_builder();
    auto ib = subsystems.engine.image_builder();

    std::string scene_name;
    if (options.scene.empty()) {
      scene_name = "assets/scenes/sponza/Sponza.gltf";
    } else {
      scene_name = options.scene;
    }
    const auto [mats, scene] = Gltf::load_from_file(ib, bb, t, scene_name);
    meshes.insert(meshes.end(), scene.begin(), scene.end());

    for (const auto& mat : mats) {
      mat->defer_deletion(subsystems.engine.global_deletion_stacks.allocator,
                          subsystems.engine.global_deletion_stacks.device);
    }

    for (const auto& mesh : meshes) {
      mesh.buffers.vertices.defer_deletion(subsystems.engine.global_deletion_stacks.allocator);
      if (mesh.buffers.indices) {
        mesh.buffers.indices->defer_deletion(subsystems.engine.global_deletion_stacks.allocator);
      }
    }

    subsystems.engine.end_transfer(std::move(t));
  }
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

    if (auto frame_opt = subsystems.engine.start_frame(); frame_opt.has_value()) {
      auto frame = frame_opt.value();
      subsystems.engine.draw(frame, meshes);

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
