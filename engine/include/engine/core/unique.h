#pragma once

#include "engine/core/macros.h"
#include "engine/core/panic.h"

#include <utility>

namespace simcoe::core {
    template<typename T, typename TDelete, T TEmpty = T()>
    struct UniqueHandle {
        SM_NOCOPY(UniqueHandle)

        constexpr UniqueHandle(T handle = TEmpty) noexcept
            : handle(handle)
        { }

        constexpr ~UniqueHandle() {
            if (handle != TEmpty) {
                kDelete(handle);
                handle = TEmpty;
            }
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
        T handle = TEmpty;

        static constexpr inline TDelete kDelete = TDelete();
    };

    template<typename T>
    struct DefaultDelete {
        constexpr void operator()(T *pData) const { delete pData; }
    };

    template<typename T>
    struct DefaultDelete<T[]> {
        constexpr void operator()(T *pData) const { delete[] pData; }
    };

    template<typename T, typename TDelete>
    struct UniquePtr;

    template<typename T, typename TDelete = DefaultDelete<T>>
    struct UniquePtr : UniqueHandle<T*, TDelete, nullptr> {
        using Super = UniqueHandle<T*, TDelete, nullptr>;
        using Super::Super;

        T *operator->() { return Super::get(); }
        const T *operator->() const { return Super::get(); }
    };

    template<typename T, typename TDelete>
    struct UniquePtr<T[], TDelete> : UniquePtr<T, TDelete> {
        using Super = UniquePtr<T, TDelete>;
        using Super::Super;

        UniquePtr(T *pData, size_t size)
            : Super(pData)
#if SM_DEBUG
            , size(size)
#endif
        { }

        UniquePtr()
            : UniquePtr(nullptr, 0)
        { }

        UniquePtr(size_t size)
            : UniquePtr(new T[size], size)
        { }

        T &operator[](size_t index) {
            verifyIndex(index);
            return Super::get()[index];
        }

        const T& operator[](size_t index) const {
            verifyIndex(index);
            return Super::get()[index];
        }

    private:
#if SM_DEBUG
        void verifyIndex(size_t index) const {
            SM_ASSERTF(index < size, "index out of bounds ({} < {})", index, size);
        }
        size_t size;
#else
        void verifyIndex(size_t) const { }
#endif
    };
}
