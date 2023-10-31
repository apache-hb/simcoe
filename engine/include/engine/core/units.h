#pragma once

#include <stddef.h>
#include <string>

namespace simcoe::core {
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

        static constexpr size_t kSizes[eLimit] = { kByte, kKilobyte, kMegabyte, kGigabyte, kTerabyte };

        static constexpr std::string_view kNames[eLimit] = {
            "b", "kb", "mb", "gb", "tb"
        };

        constexpr Memory(size_t memory = 0, Unit unit = eBytes) : bytes(memory * kSizes[unit]) { }
        static constexpr Memory fromBytes(size_t bytes) { return Memory(bytes, eBytes); }
        static constexpr Memory fromKilobytes(size_t kilobytes) { return Memory(kilobytes, eKilobytes); }
        static constexpr Memory fromMegabytes(size_t megabytes) { return Memory(megabytes, eMegabytes); }
        static constexpr Memory fromGigabytes(size_t gigabytes) { return Memory(gigabytes, eGigabytes); }
        static constexpr Memory fromTerabytes(size_t terabytes) { return Memory(terabytes, eTerabytes); }

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
