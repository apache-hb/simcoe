#pragma once

#include "engine/core/macros.h"
#include "engine/core/compiler.h"

#include <string_view>
#include <format>

namespace simcoe::core {
    struct PanicInfo {
        std::string_view file;
        std::string_view fn;
        size_t line;
    };

    [[noreturn]]
    void panic(const PanicInfo& info, std::string_view msg);
}

// debug macros

#define SM_DEBUG_ASSERTF(EXPR, ...) \
    do { \
        if (EXPR) { break; } \
        constexpr simcoe::core::PanicInfo kPanicInfo = { __FILE__, __FUNCTION__, __LINE__ }; \
        simcoe::core::panic(kPanicInfo, std::format(__VA_ARGS__)); \
    } while (false)

#if SM_DEBUG
#   define SM_ASSERTF(EXPR, ...) SM_DEBUG_ASSERTF(EXPR, __VA_ARGS__)
#else
#   define SM_ASSERTF(EXPR, ...) SM_ENSURE(EXPR)
#endif

#define SM_ASSERT(EXPR) SM_ASSERTF(EXPR, #EXPR)
#define SM_NEVER(...) SM_ASSERTF(false, __VA_ARGS__)
