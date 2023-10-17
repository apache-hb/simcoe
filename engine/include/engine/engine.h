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

    struct ReportOnce {
        template<typename F>
        void operator()(F&& func) {
            if (!bReported.test_and_set()) {
                func();
            }
        }

        void reset() {
            bReported.clear();
        }
    private:
        std::atomic_flag bReported = ATOMIC_FLAG_INIT;
    };

    namespace util {
        std::string narrow(std::wstring_view wstr);
        std::wstring widen(std::string_view str);

        std::string join(std::span<std::string_view> all, std::string_view delim);
    }
}

#define ASSERTF(expr, ...) \
    do { \
        if (!(expr)) { \
            auto msg = std::format("assert: {}", __VA_ARGS__); \
            simcoe::logError(msg); \
            throw std::runtime_error(msg); \
        } \
    } while (0)

#define ASSERT(expr) ASSERTF(expr, #expr)
