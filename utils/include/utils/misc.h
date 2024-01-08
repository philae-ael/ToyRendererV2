#pragma once

namespace utils {

template <class... T>
void ignore_unused(const T &.../*unused*/) {}

template <typename... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

}  // namespace utils
