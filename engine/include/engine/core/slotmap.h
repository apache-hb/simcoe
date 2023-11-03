#pragma once

#include "engine/core/unique.h"

#include <memory>
#include <atomic>

namespace simcoe {
    namespace detail {
        template<typename T, T Empty>
        struct SlotMapStorage {
            constexpr SlotMapStorage(size_t size)
                : size(size)
                , pSlots(size)
            {
                reset();
            }

            constexpr size_t getSize() const { return size; }

        protected:
            constexpr void reset() {
                std::fill_n(pSlots.get(), getSize(), Empty);
            }

            size_t size;
            core::UniquePtr<T[]> pSlots;
        };
    }

    template<typename T, T Empty = T()>
    struct SlotMap final : detail::SlotMapStorage<T, Empty> {
        using Super = detail::SlotMapStorage<T, Empty>;
        using Super::Super;

        enum struct Index : size_t { eInvalid = SIZE_MAX };

        SlotMap(size_t size)
            : Super(size)
        { }

        Index alloc(const T& value) {
            for (size_t i = 0; i < Super::getSize(); ++i) {
                if (Super::pSlots[i] == Empty) {
                    Super::pSlots[i] = value;
                    return Index(i);
                }
            }

            return Index::eInvalid;
        }

        void release(Index index, const T& value) {
            ASSERT(get(index) == value);
            set(index, Empty);
        }

        bool test(Index index, const T& expected) const {
            return Super::pSlots[size_t(index)] == expected;
        }

        T get(Index index) const {
            ASSERT(index != Index::eInvalid);

            return Super::pSlots[size_t(index)];
        }

        void set(Index index, const T& value) {
            ASSERT(index != Index::eInvalid);

            Super::pSlots[size_t(index)] = value;
        }
    };

    template<typename T, T Empty = T()>
    struct AtomicSlotMap final : detail::SlotMapStorage<std::atomic<T>, std::atomic<T>(Empty)> {
        using Super = detail::SlotMapStorage<std::atomic<T>, Empty>;
        using Super::Super;

        enum struct Index : size_t { eInvalid = SIZE_MAX };

        AtomicSlotMap(size_t size)
            : Super(size)
        { }

        Index alloc(const T& value) {
            for (size_t i = 0; i < Super::getSize(); i++) {
                T expected = Empty;
                if (Super::pSlots[i].compare_exchange_strong(expected, value)) {
                    return Index(i);
                }
            }

            return Index::eInvalid;
        }

        void release(Index index, const T& value) {
            ASSERT(index != Index::eInvalid);

            T expected = value;
            ASSERT(Super::pSlots[size_t(index)].compare_exchange_strong(expected, Empty));
        }

        bool test(Index index, const T& expected) const {
            ASSERT(index != Index::eInvalid);

            return Super::pSlots[size_t(index)].load() == expected;
        }

        T get(Index index) const {
            ASSERT(index != Index::eInvalid);

            return Super::pSlots[size_t(index)].load();
        }

        void set(Index index, const T& value) {
            ASSERT(index != Index::eInvalid);

            Super::pSlots[size_t(index)].store(value);
        }
    };
}
