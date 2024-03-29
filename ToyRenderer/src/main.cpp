#include <spdlog/spdlog.h>

#include "app.h"
#include "options.h"
#include "registry.h"
#include "utils/assert.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cstdlib>
#include <source_location>
#include <span>
#include <string>

[[noreturn]] void assert_violation_handler(const char* expr, const std::string& s, std::source_location loc) {
  spdlog::critical("assertion failed: {}", expr);
  spdlog::critical(s);
  spdlog::info("assertion occured in {}:{} in {}", loc.function_name(), loc.line(), loc.file_name());

  std::abort();
}

auto main(int argc, const char* argv[]) -> int {
  tr::Registry::load();

  const tr::Options args = tr::Options::from_args(std::span(argv, static_cast<std::size_t>(argc)));
  spdlog::set_level(args.debug.level);

  tr::Registry::dump();

  tr::App{args}.run();
  tr::Registry::save();
}
