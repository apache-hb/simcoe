#pragma once

#include <string_view>
#include <format>

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

    struct Region {
        Region(std::string_view start, std::string_view stop);
        ~Region();

    private:
        std::string_view stop;
    };

    namespace util {
        std::string narrow(std::wstring_view wstr);
        std::wstring widen(std::string_view str);
    }
}

#define ASSERTF(expr, ...) \
    do { \
        if (!(expr)) { \
            simcoe::logError("assert: {}. {}", #expr, std::format(__VA_ARGS__)); \
            throw std::runtime_error(#expr); \
        } \
    } while (0)

#define ASSERT(expr) \
    do { \
        if (!(expr)) { \
            simcoe::logError("assert failed: {}", #expr); \
            throw std::runtime_error(#expr); \
        } \
    } while (0)
