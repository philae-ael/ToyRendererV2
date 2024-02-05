#pragma once

#include <chrono>
#include <cstddef>
#include <ratio>
#include <span>
#include <string_view>

#include "utils/math.h"
#include "utils/misc.h"

#ifdef TIMED_INLINE_LAMBDA_DISABLE
// NOLINTNEXTLINE
#define TIMED_INLINE_LAMBDA(name) utils::inline_lambda{} + [&]()
#else
// NOLINTNEXTLINE
#define TIMED_INLINE_LAMBDA(name) utils::timed_inline_lambda{(name)} + [&]()
#endif

namespace utils {
class Timer {
 public:
  float elapsed = 0.0;
  void start() { timer_start_point = Clock::now(); }
  void stop() {
    auto timer_end_point = Clock::now();
    duration_s duration(timer_end_point - timer_start_point);
    elapsed = duration.count();
  }

 private:
  using Clock = std::chrono::high_resolution_clock;
  using duration_s = std::chrono::duration<float, std::milli>;
  Clock::time_point timer_start_point;
};

class FilteredTimer {
 public:
  void start() { timer.start(); }
  void stop() {
    timer.stop();
    filter.update(timer.elapsed);
  }

  [[nodiscard]] auto elapsed() const -> float { return filter.state; }
  [[nodiscard]] auto elapsed_raw() const -> float { return timer.elapsed; }

 private:
  Timer timer;
  math::KalmanFilter<float> filter{
      .process_covariance = 0.1F,
      .noise_covariance = 1.0F,
      .state = 0.0F,
      .covariance = 0.0F,
  };
};

// We store the data points twice to prevent a copy of the full buffer when we
// arrive at the end of the Timeline We don't want a ring buffer bc we want to
// access the data sequencially (to draw it)
template <class T, const std::size_t N = 250>
class Timeline {
 public:
  void push(T point) {
    data[index1] = point;
    data[index2] = point;

    index1 = (index1 + 1) % data.size();
    index2 = (index2 + 1) % data.size();
  }

  // if N = 1000, index2 = 1050, history is stored continously between [50;1049)
  // thus in [index1;index1 + 1000)
  auto history() -> std::span<T> { return {&data.at(std::min(index1, index2)), N}; }

 private:
  std::size_t index1 = 0;
  std::size_t index2 = N;
  std::array<T, 2 * N> data{};
};

struct timed_inline_lambda {
  timed_inline_lambda(const timed_inline_lambda &) = delete;
  timed_inline_lambda(timed_inline_lambda &&) = delete;
  auto operator=(const timed_inline_lambda &) -> timed_inline_lambda & = delete;
  auto operator=(timed_inline_lambda &&) -> timed_inline_lambda & = delete;
  explicit timed_inline_lambda(std::string_view name) : name(name) {}
  std::string_view name;
  float duration_s = 0.0;

  template <class Fn>
  auto operator+(Fn f) {
    const auto start = std::chrono::high_resolution_clock::now();
    DEFER {
      const auto end = std::chrono::high_resolution_clock::now();
      duration_s = std::chrono::duration_cast<std::chrono::duration<float>>(end - start).count();
    };

    return f();
  }
  ~timed_inline_lambda();
};

}  // namespace utils
