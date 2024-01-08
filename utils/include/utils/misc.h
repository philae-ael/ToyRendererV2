#pragma once

namespace utils {

template <class... T>
void ignore_unused(const T &.../*unused*/) {}
}  // namespace utils
