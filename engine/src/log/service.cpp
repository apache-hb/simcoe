#include "engine/log/service.h"
#include "engine/log/sinks.h"

#include "engine/config/ext/builder.h"

#include "engine/threads/service.h"

#include <iostream>

using namespace simcoe;

namespace chrono = std::chrono;

LoggingService::LoggingService() {
    addNewSink("console", new log::ConsoleSink());

    CFG_DECLARE("logging",
        CFG_FIELD_ENUM("level", &level,
            CFG_CASE("panic", log::eAssert),
            CFG_CASE("error", log::eError),
            CFG_CASE("warn", log::eWarn),
            CFG_CASE("info", log::eInfo),
            CFG_CASE("debug", log::eDebug),
        ),
        CFG_FIELD_TABLE("sinks",
            CFG_FIELD_TABLE("console",
                CFG_FIELD_BOOL("colour", &bColour)
            ),
            CFG_FIELD_TABLE("file",
                CFG_FIELD_STRING("path", &logpath)
            )
        )
    );
}

bool LoggingService::createService() {
    addNewSink("file", new log::FileSink(logpath));

    LOG_INFO("log level: {}", log::toString(level));
    return true;
}

void LoggingService::destroyService() {

}

// public interface

bool LoggingService::shouldSend(log::Level level) {
    return get()->level >= level;
}

void LoggingService::addSink(std::string_view name, log::ISink *pSink) {
    get()->addNewSink(name, pSink);
}

// private interface

void LoggingService::sendMessageAlways(log::Level msgLevel, std::string_view msg) {
    auto threadId = ThreadService::getCurrentThreadId();
    auto currentTime = chrono::system_clock::now();

    mt::read_lock guard(lock);
    for (log::ISink *pSink : sinks) {
        pSink->addLogMessage(msgLevel, threadId, currentTime, msg);
    }
}

void LoggingService::throwAssert(std::string msg) {
    sendMessage(log::eAssert, msg);
    throw std::runtime_error(msg);
}

void LoggingService::addNewSink(std::string_view id, log::ISink *pSink) {
    mt::write_lock guard(lock);
    sinks.push_back(pSink);
    lookup.emplace(id, pSink);
}
