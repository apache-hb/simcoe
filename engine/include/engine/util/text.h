#pragma once

namespace simcoe::utf8 {
    // utf8 codepoint iterator, doesnt handle invalid utf8, surrogates, etc
    struct TextIterator {
        constexpr TextIterator(const char8_t *pText, size_t offset)
            : pText(pText)
            , offset(offset)
        { }

        constexpr bool operator==(const TextIterator& other) const {
            return pText == other.pText && offset == other.offset;
        }

        constexpr bool operator!=(const TextIterator& other) const {
            return pText != other.pText || offset != other.offset;
        }

        constexpr TextIterator& operator++() {
            if ((pText[offset] & 0x80) == 0) {
                offset += 1;
            } else if ((pText[offset] & 0xE0) == 0xC0) {
                offset += 2;
            } else if ((pText[offset] & 0xF0) == 0xE0) {
                offset += 3;
            } else if ((pText[offset] & 0xF8) == 0xF0) {
                offset += 4;
            }

            return *this;
        }

        constexpr char32_t operator*() const {
            char32_t codepoint = 0;

            if ((pText[offset] & 0x80) == 0) {
                codepoint = pText[offset];
            } else if ((pText[offset] & 0xE0) == 0xC0) {
                codepoint = (pText[offset] & 0x1F) << 6;
                codepoint |= (pText[offset + 1] & 0x3F);
            } else if ((pText[offset] & 0xF0) == 0xE0) {
                codepoint = (pText[offset] & 0x0F) << 12;
                codepoint |= (pText[offset + 1] & 0x3F) << 6;
                codepoint |= (pText[offset + 2] & 0x3F);
            } else if ((pText[offset] & 0xF8) == 0xF0) {
                codepoint = (pText[offset] & 0x07) << 18;
                codepoint |= (pText[offset + 1] & 0x3F) << 12;
                codepoint |= (pText[offset + 2] & 0x3F) << 6;
                codepoint |= (pText[offset + 3] & 0x3F);
            }

            return codepoint;
        }

    private:
        const char8_t *pText;
        size_t offset;
    };

    // static utf8 string
    struct StaticText {
        constexpr StaticText(const char8_t *pText)
            : pText(pText)
            , sizeInBytes(utf8StringLength(pText))
        { }

        constexpr StaticText(const char8_t *pText, size_t size)
            : pText(pText)
            , sizeInBytes(size)
        { }

        constexpr size_t size() const {
            return sizeInBytes;
        }

        constexpr char8_t operator[](size_t index) const {
            return pText[index];
        }

        constexpr TextIterator begin() const {
            return TextIterator(pText, 0);
        }

        constexpr TextIterator end() const {
            return TextIterator(pText, sizeInBytes);
        }

    private:
        static constexpr size_t utf8StringLength(const char8_t *pText) {
            size_t length = 0;
            while (*pText++) {
                length++;
            }
            return length;
        }

        const char8_t *pText;
        size_t sizeInBytes;
    };
}
