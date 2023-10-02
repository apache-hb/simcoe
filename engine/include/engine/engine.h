#pragma once

#include <string_view>
#include <format>

namespace simcoe {
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
    }
}
