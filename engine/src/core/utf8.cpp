#include "engine/core/utf8.h"

#include "engine/log/service.h"

#include <stdint.h>

using namespace simcoe;
using namespace simcoe::utf8;

namespace {
    /**
     * @brief get the size of a utf8 string in bytes
     *
     * @param pText
     * @return constexpr size_t
     */
    constexpr size_t utf8StringLength(const char8_t *pText) {
        size_t length = 0;
        while (*pText++) {
            length += 1;
        }
        return length;
    }

    /**
     * @brief get the length of a codepoint given the first byte
     *
     * @param pText
     * @return constexpr size_t
     */
    constexpr size_t utf8CodepointSize(const char8_t *pText) {
        if ((pText[0] & 0x80) == 0) {
            return 1;
        } else if ((pText[0] & 0xE0) == 0xC0) {
            return 2;
        } else if ((pText[0] & 0xF0) == 0xE0) {
            return 3;
        } else if ((pText[0] & 0xF8) == 0xF0) {
            return 4;
        } else {
            return 0;
        }
    }

    /**
     * @brief validate a utf8 string
     *
     * @param pText
     * @param length
     * @return size_t the offset of the first invalid codepoint, or SIZE_MAX if valid
     */
    constexpr size_t utf8Validate(const char8_t *pText, size_t length) {
        size_t offset = 0;
        while (offset < length) {
            // check for invalid bytes
            char8_t byte = pText[offset];
            switch (byte) {
            case 0xFE:
            case 0xFF:
                return offset;
            default:
                break;
            }

            size_t codepointSize = utf8CodepointSize(pText + offset);
            if (codepointSize == 0) {
                return offset;
            }
            offset += codepointSize;
        }
        return SIZE_MAX;
    }
}

// text iterator

TextIterator::TextIterator(const char8_t *pText, size_t offset)
    : pText(pText)
    , offset(offset)
{ }

bool TextIterator::operator==(const TextIterator& other) const {
    return pText == other.pText && offset == other.offset;
}

bool TextIterator::operator!=(const TextIterator& other) const {
    return pText != other.pText || offset != other.offset;
}

TextIterator& TextIterator::operator++() {
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

char32_t TextIterator::operator*() const {
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

// static text

StaticText::StaticText(const char8_t *pText)
    : pText(pText)
    , sizeInBytes(utf8StringLength(pText))
{
    size_t offset = utf8Validate(pText, sizeInBytes);
    if (offset != SIZE_MAX) {
        LOG_ASSERT("invalid utf8 string at offset {}", offset);
    }
}

StaticText::StaticText(const char8_t *pText, size_t size)
    : pText(pText)
    , sizeInBytes(size)
{ }

const char8_t *StaticText::data() const { return pText; }
size_t StaticText::size() const { return sizeInBytes; }

TextIterator StaticText::begin() const {
    return TextIterator(pText, 0);
}

TextIterator StaticText::end() const {
    return TextIterator(pText, sizeInBytes);
}
