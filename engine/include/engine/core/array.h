#pragma once

#include "engine/core/unique.h"

namespace simcoe::core {
    namespace detail {
        template<typename T>
        struct DeleteArray {
            void operator()(T *pData) const { delete[] pData; }
        };
    }

    // an array with a fixed size allocated at runtime
    // std::vector<T> is not an option because it can reallocate
    // std::array<T, N> is not an option because N is a compile time constant
    // std::unique_ptr<T[]> does not retain its own size for debugging
    template<typename T>
    struct FixedArray : UniqueHandle<T*, detail::DeleteArray<T>, nullptr> {
        using Super = UniqueHandle<T*, detail::DeleteArray<T>, nullptr>;
        using Super::Super;

        constexpr FixedArray(size_t size)
            : Super(new T[size])
            , size(size)
        { }

        T &operator[](size_t index) {
            SM_ASSERTF(index < size, "index out of bounds ({} < {})", index, size);
            return Super::get()[index];
        }

        const T& operator[](size_t index) const {
            SM_ASSERTF(index < size, "index out of bounds ({} < {})", index, size);
            return Super::get()[index];
        }

        constexpr size_t getSize() const { return size; }

    private:
        size_t size;
    };
}
