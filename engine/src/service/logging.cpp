#include "engine/service/logging.h"

#include "engine/threads/service.h"

#include <iostream>

using namespace simcoe;

namespace chrono = std::chrono;

// logging

namespace {
    constexpr auto kLevelNames = std::to_array<std::string_view>({
        /*eAssert*/ "PANIC",
        /*eError*/  "ERROR",
        /*eWarn*/   "WARN",
        /*eInfo*/   "INFO",
        /*eDebug*/  "DEBUG",
    });

    constexpr auto kLevelColours = std::to_array<std::string_view>({
        /*eAssert*/ COLOUR_CYAN,
        /*eError*/  COLOUR_RED,
        /*eWarn*/   COLOUR_YELLOW,
        /*eInfo*/   COLOUR_GREEN,
        /*eDebug*/  COLOUR_PURPLE,
    });
}

// sinks

std::string logging::formatMessage(const LogMessage& msg) {
    auto ms = chrono::duration_cast<chrono::milliseconds>(msg.time.time_since_epoch()).count() % 1000;

    if (msg.name.empty()) {
        return std::format("[{:%X}.{:<3}:{:5}:{:^#6x}] {}",
            msg.time, ms,
            kLevelNames.at(msg.level),
            msg.threadId, msg.msg
        );
    }

    auto shortName = msg.name.substr(0, std::min(msg.name.size(), size_t(6)));

    return std::format("[{:%X}.{:<3}:{:5}:{:^6}] {}",
        msg.time, ms,
        kLevelNames.at(msg.level),
        shortName, msg.msg
    );
}

std::string logging::formatMessageColour(const LogMessage& msg) {
    auto ms = chrono::duration_cast<chrono::milliseconds>(msg.time.time_since_epoch()).count() % 1000;

    if (msg.name.empty()) {
        return std::format("[{:%X}.{:<3}:{}{:5}" COLOUR_RESET ":{:^#6x}] {}",
            msg.time, ms,
            kLevelColours.at(msg.level), kLevelNames.at(msg.level),
            msg.threadId, msg.msg
        );
    }

    auto shortName = msg.name.substr(0, std::min(msg.name.size(), size_t(6)));

    return std::format("[{:%X}.{:<3}:{}{:5}" COLOUR_RESET ":{:^6}] {}",
        msg.time, ms,
        kLevelColours.at(msg.level), kLevelNames.at(msg.level),
        shortName, msg.msg
    );
}

void ConsoleSink::accept(const LogMessage& msg) {
    std::lock_guard guard(lock);
    std::cout << logging::formatMessageColour(msg) << std::endl;
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

void LoggingService::sendMessage(LogLevel msgLevel, std::string_view msg) {
    LogMessage message = {
        .level = msgLevel,
        .name = ThreadService::getThreadName(),
        .threadId = ThreadService::getCurrentThreadId(),
        .time = chrono::system_clock::now(),
        .msg = msg
    };

    mt::read_lock guard(lock);
    for (ISink *pSink : sinks) {
        pSink->accept(message);
    }
}

void LoggingService::throwAssert(std::string_view msg) {
    sendMessage(eAssert, msg);
    throw std::runtime_error(std::string(msg));
}

void LoggingService::addLogSink(ISink *pSink) {
    mt::write_lock guard(lock);
    sinks.push_back(pSink);
}
