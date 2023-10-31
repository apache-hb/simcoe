#pragma once

#include "engine/core/macros.h"

#include <utility>

namespace simcoe::core {
    template<typename T, T TEmpty, void(*TDelete)(T)>
    struct UniqueHandle {
        SM_NOCOPY(UniqueHandle)

        UniqueHandle(T handle = TEmpty) noexcept
            : handle(handle)
        { }

        ~UniqueHandle() {
            if (handle != TEmpty) TDelete(handle);
        }

        UniqueHandle(UniqueHandle &&other) noexcept {
            std::swap(handle, other.handle);
        }

        UniqueHandle &operator=(UniqueHandle &&other) noexcept {
            std::swap(handle, other.handle);
            return *this;
        }

        T& get() noexcept { return handle; }
        const T& get() const noexcept { return handle; }
        explicit operator bool() const noexcept { return handle != TEmpty; }
        operator T() const noexcept { return handle; }

    private:
        T handle;
    };
}