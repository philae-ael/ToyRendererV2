#include <spdlog/spdlog.h>

#include <cstddef>

#include "app.h"
#include "options.h"
#include "registry.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

auto main(int argc, const char *argv[]) -> int {
  tr::Registry::load();

  const tr::Options args = tr::Options::from_args(std::span(argv, static_cast<std::size_t>(argc)));
  spdlog::set_level(args.debug.level);

  tr::App{args}.run();
  tr::Registry::save();
}
