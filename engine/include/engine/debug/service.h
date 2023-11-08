#pragma once

#include "engine/debug/backtrace.h"

#include "engine/service/service.h"

#include "engine/core/win32.h"

#include "engine/threads/thread.h"

#include <array>

namespace simcoe {
    namespace debug {
        void setThreadName(std::string_view name);

        std::string getResultName(HRESULT hr);
        std::string getErrorName(DWORD err = GetLastError());

        void throwLastError(std::string_view msg, DWORD err = GetLastError());

        template<typename... A>
        void throwSystemError(DWORD err, std::string_view fmt, A&&... args) {
            throwLastError(std::vformat(fmt, std::make_format_args(args...)), err);
        }

        bool isAttached();
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
        static debug::Backtrace backtrace();
    };
}
