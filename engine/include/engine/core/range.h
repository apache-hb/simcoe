#pragma once

#include <span>

namespace simcoe::core {
    template<typename T>
    struct RangeIter {
        constexpr RangeIter(T value)
            : value(value)
        { }

        constexpr T operator*() const {
            return value;
        }

        constexpr RangeIter& operator++() {
            ++value;
            return *this;
        }

        constexpr bool operator!=(const RangeIter& other) const {
            return value != other.value;
        }

    private:
        T value;
    };

    template<typename T>
    struct Range {
        constexpr Range(T last)
            : Range(T(), last)
        { }

        constexpr Range(T first, T last)
            : first(first)
            , last(last)
        { }

        constexpr RangeIter<T> begin() const {
            return RangeIter<T>(first);
        }

        constexpr RangeIter<T> end() const {
            return RangeIter<T>(last);
        }

    private:
        T first;
        T last;
    };

    template<typename TL, typename TR>
    struct ZipIter {
        constexpr ZipIter(TL left, TR right)
            : left(left)
            , right(right)
        { }

        constexpr auto operator*() const {
            struct {
                decltype(*left) left;
                decltype(*right) right;
            } it = { *left, *right };
            return it;
        }

        constexpr ZipIter& operator++() {
            ++left;
            ++right;
            return *this;
        }

        constexpr bool operator!=(const ZipIter& other) const {
            return left != other.left && right != other.right;
        }

    private:
        TL left;
        TR right;
    };

    template<typename TL, typename TR>
    struct Zip {
        constexpr Zip(TL left, TR right)
            : left(left)
            , right(right)
        { }

        constexpr auto begin() const {
            return ZipIter(left.begin(), right.begin());
        }

        constexpr auto end() const {
            return ZipIter(left.end(), right.end());
        }
    private:
        TL left;
        TR right;
    };

    template<typename TIndex = size_t>
    constexpr auto enumerate(const auto& container) {
        auto range = Range(TIndex(0), TIndex(container.size()));
        return Zip(range, container);
    }
}
