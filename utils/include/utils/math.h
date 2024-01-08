#pragma once

namespace utils::math {
inline constexpr float PI = 3.14159265359F;
inline constexpr float PI_2 = PI / 2.F;
inline constexpr float PI_4 = PI / 4.F;

struct KalmanFilter {
  // Note: we suppose that the model is **static** and one dimensionnal

  // Parameters: they should not be null
  double process_covariance;
  double noise_covariance;

  // State
  double state;
  double covariance;

  void update(double mesured) {
    double predicted_state = state;
    double predicted_covariance = covariance + process_covariance;

    double residual = mesured - predicted_state;
    double gain =
        predicted_covariance / (noise_covariance + predicted_covariance);

    state = state + gain * residual;
    covariance = (1 - gain) * predicted_covariance;
  }
};

}  // namespace utils::math
