#pragma once

#include <cstddef>
#include <fstream>
#include <vector>

#include "./assert.h"
#include "utils/cast.h"

static auto read_file(const std::string& path) -> std::vector<std::byte> {
  std::ifstream f(path, std::ios::ate | std::ios::binary);

  TR_ASSERT(f.is_open(), "failed to open file {}", path);

  std::size_t file_size = f.tellg();
  std::vector<std::byte> output(file_size);
  f.seekg(0);
  f.read(reinterpret_cast<char*>(output.data()), utils::narrow_cast<std::int64_t>(file_size));

  f.close();
  return output;
}
