#pragma once

#include <sm-config.h>

#define SM_DEPRECATED(msg) [[deprecated(msg)]]
#define SM_UNUSED [[maybe_unused]]

#define SM_NOCOPY(TYPE) \
    TYPE(const TYPE&) = delete; \
    TYPE& operator=(const TYPE&) = delete;

#define SM_NOMOVE(TYPE) \
    TYPE(TYPE&&) = delete; \
    TYPE& operator=(TYPE&&) = delete;

#define SM_ENUM_INT(TYPE, INNER) \
    using INNER = std::underlying_type_t<TYPE>; \
    constexpr TYPE operator+(TYPE lhs, TYPE rhs) { \
        return TYPE(INNER(lhs) + INNER(rhs)); \
    } \
    constexpr TYPE operator-(TYPE lhs, TYPE rhs) { \
        return TYPE(INNER(lhs) - INNER(rhs)); \
    } \
    constexpr TYPE operator*(TYPE lhs, TYPE rhs) { \
        return TYPE(INNER(lhs) * INNER(rhs)); \
    } \
    constexpr TYPE operator/(TYPE lhs, TYPE rhs) { \
        return TYPE(INNER(lhs) / INNER(rhs)); \
    } \
    constexpr TYPE operator%(TYPE lhs, TYPE rhs) { \
        return TYPE(INNER(lhs) % INNER(rhs)); \
    } \
    constexpr TYPE& operator+=(TYPE& lhs, TYPE rhs) { \
        return lhs = lhs + rhs; \
    } \
    constexpr TYPE& operator-=(TYPE& lhs, TYPE rhs) { \
        return lhs = lhs - rhs; \
    } \
    constexpr TYPE& operator*=(TYPE& lhs, TYPE rhs) { \
        return lhs = lhs * rhs; \
    } \
    constexpr TYPE& operator/=(TYPE& lhs, TYPE rhs) { \
        return lhs = lhs / rhs; \
    } \
    constexpr TYPE& operator%=(TYPE& lhs, TYPE rhs) { \
        return lhs = lhs % rhs; \
    } \
    constexpr TYPE& operator++(TYPE& value) { \
        return value = value + TYPE(1); \
    } \
    constexpr TYPE& operator--(TYPE& value) { \
        return value = value - TYPE(1); \
    } \
    constexpr std::strong_ordering operator<=>(TYPE lhs, TYPE rhs) { \
        return INNER(lhs) <=> INNER(rhs); \
    }

#define SM_ENUM_FLAGS(TYPE, INNER) \
    using INNER = std::underlying_type_t<TYPE>; \
    constexpr TYPE operator|(TYPE lhs, TYPE rhs) { \
        return TYPE(INNER(lhs) | INNER(rhs)); \
    } \
    constexpr TYPE operator&(TYPE lhs, TYPE rhs) { \
        return TYPE(INNER(lhs) & INNER(rhs)); \
    } \
    constexpr TYPE operator^(TYPE lhs, TYPE rhs) { \
        return TYPE(INNER(lhs) ^ INNER(rhs)); \
    } \
    constexpr TYPE operator~(TYPE value) { \
        return TYPE(~INNER(value)); \
    } \
    constexpr TYPE& operator|=(TYPE& lhs, TYPE rhs) { \
        return lhs = lhs | rhs; \
    } \
    constexpr TYPE& operator&=(TYPE& lhs, TYPE rhs) { \
        return lhs = lhs & rhs; \
    } \
    constexpr TYPE& operator^=(TYPE& lhs, TYPE rhs) { \
        return lhs = lhs ^ rhs; \
    }

#define SM_ENUM_FORMATTER(TYPE, INNER) \
    template<typename T> \
    struct fmt::formatter<TYPE, T> : fmt::formatter<INNER, T> { \
        template<typename FormatContext> \
        auto format(TYPE value, FormatContext& ctx) { \
            return fmt::formatter<INNER, T>::format(static_cast<INNER>(value), ctx); \
        } \
    };
