#pragma once

namespace simcoe::core {
    template<typename T>
    struct StaticText {
        constexpr StaticText(const T *pText)
            : StaticText(pText, getLength(pText))
        { }

        constexpr StaticText(const T *pText, size_t length)
            : pData(pText)
            , elemLength(length)
        {
            static_assert(at(length) == T(0), "StaticText must be null terminated");
        }

        template<size_t N>
        constexpr StaticText(const T (&text)[N])
            : StaticText(text, N - 1)
        { }

        constexpr bool operator==(const StaticText& other) const {
            if (length() != other.length()) {
                return false;
            }

            for (size_t i = 0; i < length(); i++) {
                if (pData[i] != other.pData[i]) {
                    return false;
                }
            }

            return true;
        }

        constexpr bool operator!=(const StaticText& other) const {
            if (length() != other.length()) {
                return true;
            }

            for (size_t i = 0; i < length(); i++) {
                if (pData[i] != other.pData[i]) {
                    return true;
                }
            }

            return false;
        }

        constexpr size_t length() const {
            return elemLength;
        }

        constexpr size_t sizeInBytes() const {
            return elemLength * sizeof(T);
        }

        constexpr const T &at(size_t index) const {
            return pData[index];
        }

        constexpr const T &operator[](size_t index) const {
            return pData[index];
        }

    private:
        static size_t getLength(const T *pText) {
            size_t length = 0;
            while (*pText++) {
                length++;
            }
            return length;
        }

        const T *pData;
        size_t elemLength; // length of pData in T's, not bytes
    };
}
