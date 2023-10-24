#pragma once

#include <string_view>
#include <format>
#include <span>

namespace simcoe {
    struct ILogSink {
        virtual void accept(std::string_view message) = 0;
    };

    void addSink(ILogSink* sink);

    void logInfo(std::string_view msg);
    void logWarn(std::string_view msg);
    void logError(std::string_view msg);
    void logAssert(std::string_view msg);

    template<typename... T>
    void logInfo(std::string_view msg, T&&... args) {
        logInfo(std::vformat(msg, std::make_format_args(args...)));
    }

    template<typename... T>
    void logWarn(std::string_view msg, T&&... args) {
        logWarn(std::vformat(msg, std::make_format_args(args...)));
    }

    template<typename... T>
    void logError(std::string_view msg, T&&... args) {
        logError(std::vformat(msg, std::make_format_args(args...)));
    }

    template<typename... T>
    void logAssert(std::string_view msg, T&&... args) {
        logAssert(std::vformat(msg, std::make_format_args(args...)));
    }
}

#define ASSERTF(expr, ...) \
    do { \
        if (!(expr)) { \
            auto msg = std::format(__VA_ARGS__); \
            logAssert(msg); \
        } \
    } while (0)

#define ASSERT(expr) ASSERTF(expr, #expr)
