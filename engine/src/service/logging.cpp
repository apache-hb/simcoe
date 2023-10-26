#include "engine/service/logging.h"

using namespace simcoe;

// logging

namespace {
    std::string_view getLevelName(LogLevel level) {
        switch (level) {
        case simcoe::eDebug: return "DEBUG";
        case simcoe::eInfo: return "INFO";
        case simcoe::eWarn: return "WARN";
        case simcoe::eError: return "ERROR";
        case simcoe::eAssert: return "ASSERT";
        default: return "UNKNOWN";
        }
    }
}

void LoggingService::createService() {

}

void LoggingService::destroyService() {

}

// private interface

void LoggingService::sendMessage(LogLevel level, std::string_view msg) {
    auto it = std::format("[{}:{}]: {}", DebugService::getThreadName(), getLevelName(level), msg);
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
