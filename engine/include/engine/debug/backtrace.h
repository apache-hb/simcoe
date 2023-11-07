#pragma once

#include <vector>
#include <string>

namespace simcoe::debug {
    struct StackFrame {
        std::string symbol;
        size_t pc = 0;
    };

    using Backtrace = std::vector<StackFrame>;
}
