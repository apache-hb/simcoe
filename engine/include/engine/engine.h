#pragma once

#include <string_view>

namespace simcoe {
    void logInfo(std::string_view msg);
    void logWarn(std::string_view msg);
    void logError(std::string_view msg);

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
