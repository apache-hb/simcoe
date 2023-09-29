#pragma once

#include <string_view>

namespace simcoe {
    void logInfo(std::string_view msg);
    void logWarn(std::string_view msg);
    void logError(std::string_view msg);
}
