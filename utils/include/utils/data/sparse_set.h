#pragma once

#include "hive.h"
#include "utils/types.h"

namespace utils::data {

template <class T, const std::size_t N = 256>
class sparse_set {
 public:
  struct item {
    T data;
    hive_handle_t handle;
  };

  T* get(hive_handle_t i) {
    auto index = sparse.get(i);
    if (index == nullptr) {
      return nullptr;
    }

    return &dense.at(*index).data;
  }

  T* get_unchecked(hive_index_t i) {
    auto index = sparse.get_unchecked(i);
    if (index == nullptr) {
      return nullptr;
    }
    return &dense.at(*index).data;
  }

  void swap_dense(hive_index_t i, hive_index_t j) {
    std::swap(*get_unchecked(i), *get_unchecked(j));
    std::swap(*sparse.get_unchecked(i), *sparse.get_unchecked(j));
  }

  void remove_unchecked(hive_index_t i) {
    hive_handle_t j = dense.at(length - 1).handle;
    swap_dense(i, j.hive_index);
    length -= 1;
    sparse.remove(j);
  }

  item* begin() { return dense.begin(); }
  item* end() { return dense.end(); }
  [[nodiscard]] const item* cbegin() const { return dense.begin(); }
  [[nodiscard]] const item* cend() const { return dense.cend(); }

  types::usize size() { return length; }

  std::pair<hive_handle_t, T*> create() {
    auto index = length;
    if (index == N) {
      spdlog::critical("Storage full !");
    }

    item& data = dense.at(index);
    new (&data) item{};
    length += 1;

    auto [handle, e] = sparse.create();
    *e = index;
    data.handle = handle;

    return {handle, &data.data};
  }

 private:
  std::array<item, N> dense;
  types::usize length{};

  utils::data::hive<types::usize, N> sparse;
};
}  // namespace utils::data
