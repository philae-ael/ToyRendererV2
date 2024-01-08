#include <spdlog/spdlog.h>

#include "app.h"
#include "options.h"
#include "registry.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

auto main(int argc, const char *argv[]) -> int {
  tr::Registry::load();

  const tr::Options args = tr::Options::from_args(std::span(argv, argc));
  spdlog::set_level(args.debug.level);

  tr::App{args}.run();
  tr::Registry::save();
}
