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

    namespace debug {
        void setThreadName(std::string_view name);
        std::string getThreadName();

        std::string getResultName(HRESULT hr);
        std::string getErrorName(DWORD err = GetLastError());

        void throwLastError(std::string_view msg, DWORD err = GetLastError());

        template<typename... A>
        void throwSystemError(DWORD err, std::string_view fmt, A&&... args) {
            throwLastError(std::vformat(fmt, std::make_format_args(args...)), err);
        }
    }

    struct DebugService final : IStaticService<DebugService> {
        DebugService();

        // IStaticService
        static constexpr std::string_view kServiceName = "debug";
        static constexpr std::array<std::string_view, 0> kServiceDeps = { };

        // IService
        bool createService() override;
        void destroyService() override;

        // DebugService
        static std::vector<StackFrame> backtrace();

        SM_DEPRECATED("use debug::setThreadName instead")
        static void setThreadName(std::string_view name) {
            debug::setThreadName(name);
        }

        SM_DEPRECATED("use debug::getThreadName instead")
        static std::string getThreadName() {
            return debug::getThreadName();
        }

        // win32 specific debug helpers
        SM_DEPRECATED("use debug::getResultName instead")
        static std::string getResultName(HRESULT hr) {
            return debug::getResultName(hr);
        }

        SM_DEPRECATED("use debug::getErrorName instead")
        static std::string getErrorName(DWORD err = GetLastError()) {
            return debug::getErrorName(err);
        }
    };

    SM_DEPRECATED("use debug::throwLastError or debug::throwSystemError instead")
    void throwLastError(std::string_view msg, DWORD err = GetLastError());
}
