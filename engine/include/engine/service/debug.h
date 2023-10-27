#pragma once

#include "engine/service/service.h"

#include <vector>
#include <array>

#include "engine/core/win32.h"

namespace simcoe {
    struct StackFrame {
        std::string symbol;
        size_t pc = 0;
    };

    struct DebugService final : IStaticService<DebugService> {
        // IStaticService
        static constexpr std::string_view kServiceName = "debug";
        static constexpr std::array<std::string_view, 0> kServiceDeps = { };

        // IService
        bool createService() override;
        void destroyService() override;

        // DebugService
        static std::vector<StackFrame> backtrace() {
            return USE_SERVICE(eServiceCreated, getBacktrace)();
        }

        static void setThreadName(std::string_view name);
        static std::string getThreadName();

        static std::string getResultName(HRESULT hr);
        static std::string getErrorName(DWORD err = GetLastError());

    private:
        std::vector<StackFrame> getBacktrace();
    };

    void throwError(std::string_view msg, DWORD err = GetLastError());
}