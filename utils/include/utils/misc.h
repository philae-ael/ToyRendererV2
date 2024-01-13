#pragma once

#include <cstddef>
namespace utils {

template <class... T>
void ignore_unused(const T &.../*unused*/) {}

template <typename... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

// TODO: put that in a memory.h file or smthg like that
// There is std::align but it deal with pointers
constexpr auto align(std::size_t offset, std::size_t aligmement) -> std::size_t {
  return (offset - 1 + aligmement) & (-aligmement);
}

}  // namespace utils
