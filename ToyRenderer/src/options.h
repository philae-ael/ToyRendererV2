#pragma once

#include <spdlog/spdlog.h>

namespace tr {

struct Options {
  struct {
    spdlog::level::level_enum level;
    bool renderdoc;
    bool validations_layers;
  } debug;

  static auto from_argv(std::span<const char *> args) -> Options;
};

}  // namespace tr
