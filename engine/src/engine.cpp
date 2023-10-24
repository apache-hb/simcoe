#include "engine/engine.h"

#include "engine/system/system.h"

#include <iostream>
#include <vector>

#include <windows.h>

using namespace simcoe;

namespace {
    std::vector<ILogSink*> gSinks;
    std::mutex gLock;
}

static void innerLog(std::string_view prefix, std::string_view msg) {
    std::lock_guard guard(gLock);
    auto it = std::format("[{}:{}]: {}", system::getThreadName(), prefix, msg);
    std::cout << it << std::endl;
    for (auto sink : gSinks) {
        sink->accept(it);
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

void simcoe::logAssert(std::string_view msg) {
    innerLog("ASSERT", msg);
    system::printBacktrace();
    throw std::runtime_error(std::string(msg));
}
