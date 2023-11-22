#pragma once

#include "engine/core/win32.h"
#include "engine/core/macros.h"

#include <string_view>
#include <string>

#include <limits>

namespace simcoe::units {
    struct Memory {
        enum Unit {
            eBytes,
            eKilobytes,
            eMegabytes,
            eGigabytes,
            eTerabytes,
            eLimit
        };

        static constexpr size_t kByte = 1;
        static constexpr size_t kKilobyte = kByte * 1024;
        static constexpr size_t kMegabyte = kKilobyte * 1024;
        static constexpr size_t kGigabyte = kMegabyte * 1024;
        static constexpr size_t kTerabyte = kGigabyte * 1024;

        static constexpr size_t kSizes[eLimit] = { 
            kByte, 
            kKilobyte, 
            kMegabyte, 
            kGigabyte, 
            kTerabyte 
        };

        static constexpr std::string_view kNames[eLimit] = {
            "b", "kb", "mb", "gb", "tb"
        };

        constexpr Memory(size_t memory = 0, Unit unit = eBytes) 
            : bytes(memory * kSizes[unit]) 
        { }

        constexpr size_t b() const { return bytes; }
        constexpr size_t kb() const { return bytes / kKilobyte; }
        constexpr size_t mb() const { return bytes / kMegabyte; }
        constexpr size_t gb() const { return bytes / kGigabyte; }
        constexpr size_t tb() const { return bytes / kTerabyte; }

        std::string string() const;

    private:
        size_t bytes;
    };

    enum struct Day : uint8_t {};
    enum struct Month : uint8_t {};
    enum struct Year : uint16_t {};

    struct Date {
        // time starts the day jesus was born
        constexpr Date(Day day = Day(0), Month month = Month(0), Year year = Year(0))
            : day(uint8_t(day))
            , month(uint8_t(month))
            , year(uint16_t(year))
        { }

        constexpr Day getDay() const { return Day(day); }
        constexpr Month getMonth() const { return Month(month); }
        constexpr Year getYear() const { return Year(year); }

    private:
        uint8_t day;
        uint8_t month;
        uint16_t year;
    };
}

namespace simcoe::core {
    enum struct CastError { eNone, eOverflow, eUnderflow };

    template<typename T, typename O>
    struct CastResult {
        static constexpr T kDstMin = std::numeric_limits<T>::min();
        static constexpr T kDstMax = std::numeric_limits<T>::max();

        static constexpr O kSrcMin = std::numeric_limits<O>::min();
        static constexpr O kSrcMax = std::numeric_limits<O>::max();

        T value;
        CastError error = CastError::eNone;
    };

    template<typename T, typename O>
    CastResult<T, O> checkedIntCast(O value) {
        using C = CastResult<T, O>;

        if constexpr (C::kDstMax > C::kSrcMax) {
            if (value < C::kSrcMin) {
                return { C::kDstMin, CastError::eUnderflow };
            }
        }

        if constexpr (C::kDstMin < C::kSrcMin) {
            if (value > C::kSrcMax) {
                return { C::kDstMax, CastError::eOverflow };
            }
        }

        return { static_cast<T>(value), CastError::eNone };
    }

    template<typename T, typename O>
    T intCast(O value) {
        /* paranoia */
#if SM_DEBUG
        static constexpr T kDstMin = std::numeric_limits<T>::min();
        static constexpr T kDstMax = std::numeric_limits<T>::max();

        static constexpr O kSrcMin = std::numeric_limits<O>::min();
        static constexpr O kSrcMax = std::numeric_limits<O>::max();

        if constexpr (kDstMax > kSrcMax) {
            SM_ASSERTF(value >= kSrcMin, "value {} would underflow (limit: {})", value, kSrcMin);
        }

        if constexpr (kDstMin < kSrcMin) {
            SM_ASSERTF(value <= kSrcMax, "value {} would overflow (limit: {})", value, kSrcMax);
        }
#endif

        return static_cast<T>(value);
    }

    template<typename T, typename O> requires std::is_enum_v<T> || std::is_enum_v<O>
    T enumCast(O value) {
        if constexpr (std::is_enum_v<T>) {
            return T(intCast<std::underlying_type_t<T>>(value));
        } else {
            return T(value);
        }
    }

    template<typename T>
    constexpr T nextPowerOf2(T value) {
        T result = 1;
        while (result < value) {
            result <<= 1;
        }

        return result;
    }
}
