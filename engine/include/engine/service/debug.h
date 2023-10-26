#pragma once

#include "engine/service/platform.h"

#include <vector>

namespace simcoe {
    struct StackFrame {
        std::string symbol;
        size_t pc = 0;
    };

    // TODO: does debug depend on platform or does platform depend on debug
    struct DebugService final : IStaticService<DebugService> {
        // IStaticService
        static constexpr std::string_view kServiceName = "debug";
        static constexpr std::array<std::string_view, 1> kServiceDeps = { PlatformService::kServiceName };

        // IService
        void createService() override;
        void destroyService() override;

        // DebugService
        static std::vector<StackFrame> backtrace() {
            return USE_SERVICE(getBacktrace)();
        }

    private:
        std::vector<StackFrame> getBacktrace();
    };
}
