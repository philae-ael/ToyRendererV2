#pragma once

#include <array>
#include <cstddef>

#include "utils/assert.h"
#include "utils/types.h"

namespace utils::data {

// A stack that lives on an array and thus is static
template <types::trivial T, const types::usize N>
class static_stack {
 private:
  std::array<T, N> inner;
  types::usize item_count{0};

 public:
  types::usize size() { return item_count; }

  void push(T item) {
    TR_ASSERT(item_count < N, "attempt to push in a full stack");
    inner.at(item_count) = item;
    item_count += 1;
  }
  void pop() {
    TR_ASSERT(item_count > 1, "attempt to pop an empty stack");
    item_count -= 1;
  }

  void clear() { item_count = 0; }

  auto begin() { return inner.begin(); }
  auto end() { return inner.begin() + item_count; }
  [[nodiscard]] auto cbegin() const { return inner.cbegin(); }
  [[nodiscard]] auto cend() const { return inner.cbegin() + item_count; }
};

template class static_stack<int, 5>;

}  // namespace utils::data
