#include "engine/core/strings.h"

#include "engine/core/win32.h"

using namespace simcoe;
using namespace simcoe::util;

std::string util::narrow(std::wstring_view wstr) {
    std::string result(wstr.size() + 1, '\0');
    size_t size = result.size();

    errno_t err = wcstombs_s(&size, result.data(), result.size(), wstr.data(), wstr.size());
    if (err != 0) {
        return "";
    }

    result.resize(size - 1);
    return result;
}

std::wstring util::widen(std::string_view str) {
    auto needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
    std::wstring result(needed, '\0');
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), result.data(), (int)result.size());
    return result;
}

template<typename T>
std::string joinInner(std::span<const T> all, std::string_view delim) {
    std::string result;
    for (size_t i = 0; i < all.size(); i++) {
        result += all[i];
        if (i != all.size() - 1) {
            result += delim;
        }
    }

    return result;
}

std::string util::join(std::span<const std::string_view> all, std::string_view delim) {
    return joinInner(all, delim);
}

std::string util::join(std::span<const std::string> all, std::string_view delim) {
    return joinInner(all, delim);
}


SplitView::SplitView(std::string_view text, std::string_view delim)
    : text(text)
    , delim(delim)
{ }

SplitViewIter SplitView::begin() const {
    return SplitViewIter(text, delim);
}

SplitViewIter SplitView::end() const {
    return SplitViewIter({}, delim);
}
