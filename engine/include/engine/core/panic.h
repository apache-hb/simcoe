#pragma once

#include <string_view>
#include <format>

namespace simcoe::core {
    struct PanicInfo {
        std::string_view file;
        std::string_view fn;
        size_t line;
    };

    void panic(const PanicInfo& info, std::string_view msg);
}

// debug macros

// TODO: have multiple asserts, one for debug, one for release
// TODO: in release mode asserts should be turned into ensures for better performance

#define ASSERTF(EXPR, ...) \
    do { \
        if (EXPR) { break; } \
        constexpr simcoe::core::PanicInfo kPanicInfo = { __FILE__, __FUNCTION__, __LINE__ }; \
        simcoe::core::panic(kPanicInfo, std::format(__VA_ARGS__)); \
    } while (false)

#define ASSERT(EXPR) ASSERTF(EXPR, #EXPR)
#define NEVER(...) ASSERTF(false, __VA_ARGS__)
