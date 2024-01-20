#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <vector>

#include "./assert.h"
#include "utils/cast.h"

template <class T>
static auto read_file(const std::string& path) -> std::vector<T> {
  std::ifstream f(path, std::ios::ate | std::ios::binary);

  TR_ASSERT(f.is_open(), "failed to open file {}", path);

  std::size_t file_size = f.tellg();
  std::vector<T> output(file_size);
  f.seekg(0);
  f.read(reinterpret_cast<char*>(output.data()), utils::narrow_cast<std::int64_t>(file_size));

  f.close();
  return output;
}
