#pragma once

#include "engine/math/math.h"

#include "vendor/fmtlib/fmt.h"

template<typename T>
struct fmt::formatter<simcoe::math::Vec2<T>> : fmt::formatter<std::string_view> {
    auto format(simcoe::math::Vec2<T> const &v, auto &ctx) {
        return fmt::formatter<std::string_view>::format(
            fmt::format("{} {}", v.x, v.y), ctx
        );
    }
};

template<typename T>
struct fmt::formatter<simcoe::math::Vec3<T>> : fmt::formatter<std::string_view> {
    auto format(simcoe::math::Vec3<T> const &v, auto &ctx) {
        return fmt::formatter<std::string_view>::format(
            fmt::format("{} {} {}", v.x, v.y, v.z), ctx
        );
    }
};

template<typename T>
struct fmt::formatter<simcoe::math::Vec4<T>> : fmt::formatter<std::string_view> {
    auto format(simcoe::math::Vec4<T> const &v, auto &ctx) {
        return fmt::formatter<std::string_view>::format(
            fmt::format("{} {} {} {}", v.x, v.y, v.z, v.w), ctx
        );
    }
};
