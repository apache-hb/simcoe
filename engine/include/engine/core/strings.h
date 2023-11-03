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
}
