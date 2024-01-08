#include <spdlog/spdlog.h>

#include "app.h"
#include "options.h"
#include "registry.h"

auto main(int argc, const char *argv[]) -> int {
  const tr::Options args = tr::Options::from_args(std::span(argv, argc));
  spdlog::set_level(args.debug.level);

  tr::Registry::load();
  tr::App{args}.run();
  tr::Registry::save();
}
