#include "engine/service/logging.h"

#include <iostream>
#include <unordered_map>

using namespace simcoe;

// logging

namespace {
    const std::unordered_map<LogLevel, std::string_view> kLevelNames = {
        { eAssert, "ASSERT" },
        { eError, "ERROR" },
        { eWarn, "WARN" },
        { eInfo, "INFO" },
        { eDebug, "DEBUG" },
    };
}

// sinks

void ConsoleSink::accept(std::string_view msg) {
    std::cout << msg << std::endl;
}

LoggingService::LoggingService() {
    addLogSink(new ConsoleSink());
}

bool LoggingService::createService() {
    LOG_INFO("log level: {}", kLevelNames.at(level));
    return true;
}

void LoggingService::destroyService() {

}

// private interface

void LoggingService::sendMessage(LogLevel level, std::string_view msg) {
    auto it = std::format("[{}:{}]: {}", DebugService::getThreadName(), kLevelNames.at(level), msg);
    std::lock_guard guard(lock);
    for (ISink *pSink : sinks) {
        pSink->accept(it);
    }
}

void LoggingService::throwAssert(std::string_view msg) {
    sendMessage(eAssert, msg);
    throw std::runtime_error(std::string(msg));
}

void LoggingService::addLogSink(ISink *pSink) {
    std::lock_guard guard(lock);
    sinks.push_back(pSink);
}
