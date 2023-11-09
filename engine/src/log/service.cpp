#include "engine/log/service.h"
#include "engine/core/error.h"
#include "engine/log/sinks.h"

#include "engine/config/system.h"

#include "engine/threads/service.h"

#include <iostream>

using namespace simcoe;

namespace chrono = std::chrono;

const config::ConfigFlagMap kLevelNames = {
    { "assert", log::eAssert },
    { "error", log::eError },
    { "warn", log::eWarn },
    { "info", log::eInfo },
    { "debug", log::eDebug },
};

config::ConfigValue<log::Level> cfgLogLevel("logging", "level", "default logging level", log::eInfo, kLevelNames, config::eDynamic);

namespace {
    // log sinks
    mt::shared_mutex lock;
    std::vector<log::ISink*> sinks;
    std::unordered_map<std::string_view, log::ISink*> lookup;
}

LoggingService::LoggingService() {
    addSink("console", new log::ConsoleSink());
    addSink("file", new log::FileSink());
}

bool LoggingService::createService() {
    LOG_INFO("log level: {}", log::toString(cfgLogLevel.getCurrentValue()));
    return true;
}

void LoggingService::destroyService() {

}

// public interface

bool LoggingService::shouldSend(log::Level level) {
    return cfgLogLevel.getCurrentValue() >= level;
}

void LoggingService::addSink(std::string_view name, log::ISink *pSink) {
    mt::write_lock guard(lock);
    sinks.push_back(pSink);
    lookup.emplace(name, pSink);
}

// private interface

void LoggingService::sendMessageAlways(log::Level msgLevel, std::string_view msg) {
    auto threadId = ThreadService::getCurrentThreadId();
    auto currentUtcTime = chrono::utc_clock::to_sys(chrono::utc_clock::now());

    mt::read_lock guard(lock);
    for (log::ISink *pSink : sinks) {
        pSink->addLogMessage(msgLevel, threadId, currentUtcTime, msg);
    }
}

void LoggingService::throwAssert(std::string msg) {
    sendMessage(log::eAssert, msg);
    core::throwFatal(msg);
}
