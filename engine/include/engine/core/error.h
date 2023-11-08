#pragma once

#include <format>

#include "engine/debug/backtrace.h"

namespace simcoe::core {
    struct Error {
        template<typename... A>
        Error(bool bFatal, std::string_view msg, A&&... args) noexcept
            : Error(bFatal, std::vformat(msg, std::make_format_args(args...)))
        { }

        Error(bool bFatal, std::string msg) noexcept;

        /// can this error be reasonably recovered from?
        bool recoverable() const noexcept { return bFatal; }

        // get the error message
        std::string_view what() const noexcept { return message; }

        const debug::Backtrace& getStacktrace() const noexcept { return stacktrace; }

    private:
        /// can this error be reasonably recovered from?
        /// failing to load a texture is an example of an error that can be recovered from
        /// but an integer overflow is not recoverable
        bool bFatal = false;

        /// the error message
        std::string message;

        // the stacktrace at the time of the error
        debug::Backtrace stacktrace;
    };

    template<typename... A>
    [[noreturn]] void throwFatal(std::string_view msg, A&&... args) {
        throw Error(true, msg, std::forward<A>(args)...);
    }

    template<typename... A>
    [[noreturn]] void throwNonFatal(std::string_view msg, A&&... args) {
        throw Error(false, msg, std::forward<A>(args)...);
    }
}

#define HR_CHECK(expr) \
    do { \
        if (HRESULT hr = (expr); FAILED(hr)) { \
            core::throwNonFatal("{} ({})", #expr, debug::getResultName(hr)); \
        } \
    } while (false)
