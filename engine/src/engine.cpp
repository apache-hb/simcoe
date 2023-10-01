#include "engine/engine.h"

#include <iostream>

using namespace simcoe;

static void innerLog(std::string_view prefix, std::string_view msg) {
    std::cout << prefix << ": " << msg << std::endl;
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

Region::Region(std::string_view start, std::string_view stop) : stop(stop) {
    logInfo(start);
}

Region::~Region() {
    logInfo(stop);
}
