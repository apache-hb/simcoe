#pragma once

#include "engine/core/unique.h"

#include <memory>
#include <bit>
namespace simcoe::core {
    namespace detail {
        template<typename T, typename Super>
        struct BitMapStorage {
            enum struct Index : size_t { eInvalid = SIZE_MAX };

            constexpr BitMapStorage(size_t bits)
                : size(bits)
                , pBits(wordCount())
            {
                reset();
            }

            constexpr size_t countSetBits() const { 
                size_t count = 0;
                for (size_t i = 0; i < wordCount(); i++) {
                    count += std::popcount(pBits[i]);
                }
                return count;
            }

            constexpr size_t getTotalBits() const { return size; }
            constexpr size_t getCapacity() const { return wordCount() * kBitPerWord; }

            constexpr bool test(Index index) const {
                verifyIndex(index);

                return pBits[getWord(size_t(index))] & getMask(size_t(index));
            }

            constexpr Index alloc() {
                Super *self = static_cast<Super*>(this);
                for (size_t i = 0; i < getTotalBits(); i++) {
                    if (self->testSet(i)) {
                        return Index(i);
                    }
                }

                return Index::eInvalid;
            }

            constexpr void release(Index index) {
                clear(size_t(index));
            }

            constexpr void reset() {
                std::fill_n(pBits.get(), wordCount(), 0);
            }

            constexpr static inline size_t kBitPerWord = sizeof(T) * CHAR_BIT;

        protected:
            constexpr void set(size_t index) {
                verifyIndex(Index(index));
                pBits[getWord(index)] |= getMask(index);
            }

            constexpr void clear(size_t index) {
                verifyIndex(Index(index));
                pBits[getWord(index)] &= ~getMask(index);
            }

        protected:
            constexpr T getMask(size_t bit) const { return T(1) << (bit % kBitPerWord); }
            constexpr size_t getWord(size_t bit) const { return bit / kBitPerWord; }
            constexpr size_t wordCount() const { return (getTotalBits() / kBitPerWord) + 1; }

            size_t size;
            core::UniquePtr<T[]> pBits;

            void verifyIndex(Index index) const {
                SM_ASSERTF(index != Index::eInvalid, "invalid index");
                SM_ASSERTF(size_t(index) <= getTotalBits(), "bit {} is out of bounds", size_t(index));
            }
        };
    }

    struct BitMap final : detail::BitMapStorage<std::uint64_t, BitMap> {
        using Super = detail::BitMapStorage<std::uint64_t, BitMap>;
        using Super::BitMapStorage;

        bool testSet(size_t index);
    };

    struct AtomicBitMap final : detail::BitMapStorage<std::atomic_uint64_t, AtomicBitMap> {
        using Super = detail::BitMapStorage<std::atomic_uint64_t, AtomicBitMap>;
        using Super::BitMapStorage;

        bool testSet(size_t index);
    };
}
