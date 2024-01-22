#pragma once

#include <array>
#include <cstddef>

#include "utils/assert.h"
#include "utils/types.h"

namespace utils::data {

// A stack that lives on an array and thus is static
template <types::trivial T, const std::size_t N>
class static_stack {
 private:
  std::array<T, N> inner{};
  std::size_t item_count{0};

 public:
  [[nodiscard]] auto size() const -> std::size_t { return item_count; }
  [[nodiscard]] auto empty() const -> bool { return item_count == 0; }

  void push_back(T item) {
    TR_ASSERT(item_count < N, "attempt to push in a full stack");
    inner.at(item_count) = item;
    item_count += 1;
  }
  void pop_back() {
    TR_ASSERT(item_count > 1, "attempt to pop an empty stack");
    item_count -= 1;
  }

  void clear() { item_count = 0; }
  auto data() -> T* { return inner.data(); }
  auto data() const -> const T* { return inner.data(); }

  auto begin() { return inner.begin(); }
  auto end() { return inner.begin() + item_count; }
  [[nodiscard]] auto cbegin() const { return inner.cbegin(); }
  [[nodiscard]] auto cend() const { return inner.cbegin() + item_count; }
};
}  // namespace utils::data
