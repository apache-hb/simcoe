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
