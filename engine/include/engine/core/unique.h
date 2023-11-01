#pragma once

#include "engine/core/macros.h"

#include <utility>

namespace simcoe::core {
    template<typename T, typename O>
    concept ConstDeleter = requires(const T& it) {
        { it(O()) } -> std::same_as<void>;
    } && std::is_empty_v<T>;

    template<typename T, T TEmpty, ConstDeleter<T> TDelete>
    struct UniqueHandle {
        SM_NOCOPY(UniqueHandle)

        constexpr UniqueHandle(T handle = TEmpty) noexcept
            : handle(handle)
        { }

        constexpr ~UniqueHandle() {
            if (handle != TEmpty) kDelete(handle);
        }

        constexpr UniqueHandle(UniqueHandle &&other) noexcept {
            std::swap(handle, other.handle);
        }

        constexpr UniqueHandle &operator=(UniqueHandle &&other) noexcept {
            std::swap(handle, other.handle);
            return *this;
        }

        constexpr T& get() noexcept { return handle; }
        constexpr const T& get() const noexcept { return handle; }
        constexpr explicit operator bool() const noexcept { return handle != TEmpty; }
        constexpr operator T() const noexcept { return handle; }

    private:
        T handle;

        [[no_unique_address]] TDelete kDelete = TDelete();
    };
}
