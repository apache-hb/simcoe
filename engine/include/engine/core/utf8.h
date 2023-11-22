#pragma once

#include <cstdlib>
#include <iterator>

namespace simcoe::utf8 {
    // utf8 codepoint iterator, doesnt handle invalid utf8, surrogates, etc
    struct TextIterator {
        using difference_type = ptrdiff_t;
        using reference = char32_t;
        using value_type = char32_t;
        using pointer = const char32_t*;
        using iterator_category = std::forward_iterator_tag;

        TextIterator(const char8_t *pText, size_t offset);

        bool operator==(const TextIterator& other) const;
        bool operator!=(const TextIterator& other) const;

        TextIterator& operator++();

        char32_t operator*() const;

    private:
        const char8_t *pText;
        size_t offset;
    };

    // static utf8 string
    struct StaticText {
        StaticText(const char8_t *pText);

        StaticText(const char8_t *pText, size_t size);

        const char8_t *data() const;
        size_t size() const;

        TextIterator begin() const;
        TextIterator end() const;

    private:
        const char8_t *pText;
        size_t sizeInBytes;
    };
}
