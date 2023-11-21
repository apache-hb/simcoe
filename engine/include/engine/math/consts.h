#pragma once

#include <numbers>

namespace simcoe::math {
    template<typename T>
    constexpr T kPi = std::numbers::pi_v<T>;

    template<typename T>
    constexpr T kRadToDeg = T(180) / kPi<T>;

    template<typename T>
    constexpr T kDegToRad = kPi<T> / T(180);
}
