#pragma once

#include <filesystem>

namespace simcoe {
    // just export the std::filesystem namespace as fs
    namespace fs = std::filesystem; // NOLINT
}
