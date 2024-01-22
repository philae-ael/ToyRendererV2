#pragma once

#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <optional>
#include <vector>

#include "utils/cast.h"

template <class T>
static auto read_file(const std::string& path) -> std::optional<std::vector<T>> {
  std::ifstream f(path, std::ios::ate | std::ios::binary);

  if (!f.is_open()) {
    spdlog::error("failed to open file {}", path);
    return std::nullopt;
  }

  const auto file_size = static_cast<std::size_t>(f.tellg());
  std::vector<T> output(file_size);
  f.seekg(0);
  f.read(reinterpret_cast<char*>(output.data()), utils::narrow_cast<std::int64_t>(file_size));

  f.close();
  return output;
}
