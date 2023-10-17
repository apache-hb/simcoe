#include "engine/engine.h"

#include <iostream>
#include <vector>

#include <windows.h>

using namespace simcoe;

std::vector<ILogSink*> gSinks;
std::mutex lock;

static void innerLog(std::string_view prefix, std::string_view msg) {
    std::lock_guard guard(lock);
    std::cout << prefix << ": " << msg << std::endl;
    for (auto sink : gSinks) {
        sink->accept(std::format("{}: {}", prefix, msg));
    }
}

void simcoe::addSink(ILogSink* sink) {
    gSinks.push_back(sink);
}

void simcoe::logInfo(std::string_view msg) {
    innerLog("INFO", msg);
}

void simcoe::logWarn(std::string_view msg) {
    innerLog("WARN", msg);
}

void simcoe::logError(std::string_view msg) {
    innerLog("ERROR", msg);
}

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

std::string util::join(std::span<std::string_view> all, std::string_view delim) {
    std::string result;
    for (size_t i = 0; i < all.size(); i++) {
        result += all[i];
        if (i != all.size() - 1) {
            result += delim;
        }
    }

    return result;
}
