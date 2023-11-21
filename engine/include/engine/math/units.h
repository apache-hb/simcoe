#pragma once

#include "engine/math/consts.h"

namespace simcoe::math {
    template<typename T> struct Degrees;
    template<typename T> struct Radians;

    template<typename T>
    struct Degrees {
        using Radians = Radians<T>;

        constexpr Degrees(T value = T(0)) 
            : value(value) 
        { }

        constexpr Degrees(Radians rad) 
            : value(rad * T(180) / T(kPi<T>)) 
        { }

        constexpr operator T() const { return value; }

        T value;
    };

    template<typename T>
    struct Radians {
        using Degrees = Degrees<T>;

        constexpr Radians(T value = T(0)) 
            : value(value) 
        { }

        constexpr Radians(Degrees deg) 
            : value(deg * T(kPi<T>) / T(180)) 
        { }

        constexpr operator T() const { return value; }

        T value;
    };
}
