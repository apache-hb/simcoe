#pragma once

#include "engine/math/math.h"

#include <format>

template<typename T, typename CharT>
struct std::formatter<simcoe::math::Vec2<T>, CharT> : std::formatter<std::string, CharT> {
    auto format(simcoe::math::Vec2<T> const &v, auto &ctx) {
        return std::formatter<std::string, CharT>::format(
            std::format("{} {}", v.x, v.y), ctx
        );
    }
};

template<typename T, typename CharT>
struct std::formatter<simcoe::math::Vec3<T>, CharT> : std::formatter<std::string, CharT> {
    auto format(simcoe::math::Vec3<T> const &v, auto &ctx) {
        return std::formatter<std::string, CharT>::format(
            std::format("{} {} {}", v.x, v.y, v.z), ctx
        );
    }
};

template<typename T, typename CharT>
struct std::formatter<simcoe::math::Vec4<T>, CharT> : std::formatter<std::string, CharT> {
    auto format(simcoe::math::Vec4<T> const &v, auto &ctx) {
        return std::formatter<std::string, CharT>::format(
            std::format("{} {} {} {}", v.x, v.y, v.z, v.w), ctx
        );
    }
};
