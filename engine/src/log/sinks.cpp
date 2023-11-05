#include "engine/log/sinks.h"
#include "engine/log/service.h"

#include "engine/config/ext/builder.h"

#include <iostream>

using namespace simcoe;
using namespace simcoe::log;

ConsoleSink::ConsoleSink(bool bColour)
    : ISink(true)
    , bColour(bColour)
{ }

void ConsoleSink::accept(const Message& msg) {
    std::lock_guard guard(mutex);
    auto it = bColour ? log::formatMessageColour(msg) : log::formatMessage(msg);
    std::cout << it << "\n";
}

bool ConsoleSink::hasColourSupport() {
    DWORD dwMode = 0;
    if (!GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &dwMode)) {
        return false;
    }

    return dwMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING;
}

FileSink::FileSink(std::string path)
    : ISink(true)
    , os(path)
{
    if (!os.is_open()) {
        LOG_WARN("failed to open log file: {}", path);
    }
}

void FileSink::accept(const Message& msg) {
    std::lock_guard guard(mutex);
    if (!os.is_open()) {
        return;
    }

    auto it = log::formatMessage(msg);
    os << it << "\n";
}
