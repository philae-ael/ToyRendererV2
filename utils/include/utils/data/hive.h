#pragma once

#include <array>
#include <optional>

#include "utils/assert.h"
#include "utils/cast.h"
#include "utils/types.h"

// based on what i read on the future std::hive
// + some personal (maybe bad) choices
//
// Choices done:
// - a generation id
// so that handles are more like weak pointers that
// pointers to memory while preventing increase in memory
//
// - hive.data is a vector and not a list to allow O(1) lookup
// insertion should still be (amortized) O(1) but slower than with a list (not
// tested)
//
// Features:
// - Stable handles,
// - Iterate as fast as a vector (well if N is big enough)
// - creation, deletion, get in O(1)

namespace utils::data {

struct hive_index_t {
  union {
    struct {
      std::uint16_t chunk;
      std::uint16_t chunk_index;
    };
    std::uint32_t index;
  };
};

struct hive_handle_t {
  std::uint32_t generation;
  hive_index_t hive_index;
};

struct chunk_handle_t {
  std::uint32_t generation;
  std::uint16_t index;
};

template <types::trivial T, const std::size_t N>
class chunk {
  struct Data {
    union {
      T data;
      std::optional<hive_index_t> next_free;
    };
    std::uint32_t generation{};
    bool valid{};
  };

  std::array<Data, N> data{};
  std::size_t initialized{0};

 public:
  auto get(chunk_handle_t handle) -> T* {
    auto& d = data.at(handle.index);
    if (!d.valid || d.generation != handle.generation) {
      return nullptr;
    }
    return &d.data;
  }

  auto get_unchecked(std::uint16_t index) -> T* { return &data.at(index).data; }

  auto create(hive_index_t index) -> std::tuple<hive_handle_t, std::optional<hive_index_t>, T*> {
    auto& d = data.at(index.chunk_index);
    auto next_index = d.next_free;

    if (index.chunk_index >= initialized) {
      TR_ASSERT(index.chunk_index == initialized, "invalid chunk_index!");
      d.generation = 0;

      if (initialized <= N) {
        next_index = {
            .chunk = index.chunk,
            .chunk_index = utils::narrow_cast<std::uint16_t>(initialized + 1),
        };
      } else {
        next_index = std::nullopt;
      }
      initialized++;
    }

    new (&d.data) T{};
    d.valid = true;
    return {
        {.generation = d.generation, .hive_index = index},
        next_index,
        &d.data,
    };
  }

  void remove(chunk_handle_t handle, std::optional<hive_index_t> next_free) {
    auto& d = data.at(handle.index);
    d.next_free = next_free;
    d.generation += 1;
    d.valid = false;
  }
};

template <types::trivial T, const std::size_t N = 256>
class hive {
 private:
  std::vector<chunk<T, N>> inner;
  std::optional<hive_index_t> next_free;

 public:
  auto get(hive_handle_t handle) -> T* {
    if (handle.hive_index.chunk >= inner.size()) {
      return nullptr;
    }
    return inner[handle.hive_index.chunk].get({
        .generation = handle.generation,
        .index = handle.hive_index.chunk_index,
    });
  }
  auto get_unchecked(hive_index_t index) -> T* {
    if (index.chunk >= inner.size()) {
      return nullptr;
    }
    return inner[index.chunk].get_unchecked(index.chunk_index);
  }

  auto create() -> std::pair<hive_handle_t, T*> {
    if (!next_free.has_value()) {
      inner.emplace_back();
      next_free = {
          .chunk = utils::narrow_cast<std::uint16_t>(inner.size() - 1),
          .chunk_index = 0,
      };
    }

    auto [handle, new_next_free, data] = inner[next_free->chunk].create(*next_free);

    next_free = new_next_free;
    return {handle, data};
  }

  void remove(hive_handle_t handle) {
    inner[handle.hive_index.chunk].remove(
        {
            .generation = handle.generation,
            .index = handle.hive_index.chunk_index,
        },
        next_free);
    next_free = handle.hive_index;
  }
};

template class hive<std::uint32_t>;
}  // namespace utils::data
