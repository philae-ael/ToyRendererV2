#pragma once

#include <cstddef>
#include <span>
#include <utility>
namespace utils {

template <class T, class U>
constexpr auto narrow_cast(U&& u) -> T {
  return static_cast<T>(std::forward<U>(u));
}

template <class T>
constexpr auto array_decay_cast(std::span<T> s) -> T* {
  return s.data();
}

template <typename T>
constexpr auto to_array(std::span<T, 0> /*unused*/) -> std::array<T, 0> {
  return {};
}

// implementation from https://en.cppreference.com/w/cpp/container/array/to_array + size 0 added, from me
namespace detail {
template <class T, std::size_t N, std::size_t... I>
constexpr auto to_array_impl(T (&a)[N], std::index_sequence<I...> /*unused*/)  // NOLINT
    -> std::array<std::remove_cv_t<T>, N> {
  return {{a[I]...}};
}

template <class T, std::size_t N, std::size_t... I>
constexpr auto to_array_impl(T (&&a)[N], std::index_sequence<I...>)  // NOLINT
    -> std::array<std::remove_cv_t<T>, N> {
  return {{std::move(a[I])...}};
}
}  // namespace detail

template <class T, std::size_t N>
constexpr auto to_array(T (&a)[N]) -> std::array<std::remove_cv_t<T>, N> {  // NOLINT
  return detail::to_array_impl(a, std::make_index_sequence<N>{});
}

template <class T, std::size_t N>
constexpr auto to_array(T (&&a)[N]) -> std::array<std::remove_cv_t<T>, N> {  // NOLINT
  return detail::to_array_impl(std::move(a), std::make_index_sequence<N>{});
}
}  // namespace utils
