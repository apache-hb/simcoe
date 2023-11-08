#pragma once

#include <string>
#include <string_view>
#include <span>

namespace simcoe::util {
    /**
     * @brief convert a utf16 string to utf8
     *
     * @param wstr the utf16 string
     * @return std::string the utf8 string
     */
    std::string narrow(std::wstring_view wstr);

    /**
     * @brief convert a utf8 string to utf16
     *
     * @param str the utf8 string
     * @return std::wstring the utf16 string
     */
    std::wstring widen(std::string_view str);

    std::string join(std::span<const std::string> all, std::string_view delim);
    std::string join(std::span<const std::string_view> all, std::string_view delim);

    // create a view over a string that can be iterated over in chunks
    // seperated by a delimiter
    /**
     * auto text = "hello/world/test";
     * for (auto chunk : SplitView(text, "/")) {
     *     LOG_INFO("chunk: {}", chunk);
     * }
     * should print hello world test
     */
    struct SplitViewIter {
        SplitViewIter(std::string_view text, std::string_view delim)
            : text(text)
            , delim(delim)
        {
            advance();
        }

        std::string_view operator*() const {
            return chunk;
        }
        SplitViewIter& operator++() {
            advance();
            return *this;
        }
        bool operator!=(const SplitViewIter& other) const {
            return chunk != other.chunk;
        }
    private:
        void advance() {
            auto pos = text.find(delim);
            if (pos == std::string_view::npos) {
                chunk = text;
                text = {};
                return;
            }

            chunk = text.substr(0, pos);
            text = text.substr(pos + delim.size());
        }

        std::string_view text;
        std::string_view delim;
        std::string_view chunk;
    };

    struct SplitView {
        SplitView(std::string_view text, std::string_view delim);

        SplitViewIter begin() const;
        SplitViewIter end() const;
    private:
        std::string_view text;
        std::string_view delim;
    };
}
