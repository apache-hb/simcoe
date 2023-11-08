#include "engine/log/sinks.h"
#include "engine/log/service.h"

#include <iostream>

#include "engine/config/system.h"

using namespace simcoe;
using namespace simcoe::log;

config::ConfigValue<bool> cfgLogColour("logging/console", "colour", "enable coloured console output", true);
config::ConfigValue<std::string> cfgLogPath("logging/file", "path", "path to log file", "log.txt");

bool hasColourSupport() {
    DWORD dwMode = 0;
    if (!GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &dwMode)) {
        return false;
    }

    return dwMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING;
}

ConsoleSink::ConsoleSink()
    : ISink(true)
    , bColour(hasColourSupport() && cfgLogColour.getValue())
{ }

void ConsoleSink::accept(const Message& msg) {
    std::lock_guard guard(mutex);
    auto it = bColour ? log::formatMessageColour(msg) : log::formatMessage(msg);
    std::cout << it << "\n";
}

FileSink::FileSink()
    : ISink(true)
    , os(cfgLogPath.getValue())
{
    if (!os.is_open()) {
        LOG_WARN("failed to open log file: {}", cfgLogPath.getValue());
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
