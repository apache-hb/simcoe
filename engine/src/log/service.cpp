#include "engine/log/service.h"
#include "engine/core/error.h"
#include "engine/log/sinks.h"

#include "engine/config/system.h"

#include "engine/threads/service.h"
#include "engine/threads/queue.h"
#include "engine/threads/messages.h"

#include <iostream>
#include <queue>

using namespace simcoe;

namespace chrono = std::chrono;

const config::ConfigEnumMap kLevelNames = {
    { "assert", log::eAssert },
    { "error", log::eError },
    { "warn", log::eWarn },
    { "info", log::eInfo },
    { "debug", log::eDebug },
};

config::ConfigValue<log::Level> cfgLogLevel("logging", "level", "default logging level", log::eInfo, kLevelNames, config::eDynamic);

config::ConfigValue<size_t> cfgLogQueueSize("logging/worker", "queue_size", "amount of messages to queue before blocking", 1024);
config::ConfigValue<size_t> cfgLogQueueInterval("logging/worker", "wait_interval", "amount of time to wait before checking for more messages (in ms)", 50);

struct LogMessage {
    log::Level level;
    threads::ThreadId id;
    log::MessageTime time;
    std::string msg;
};

using LogQueue = mt::BlockingMessageQueue<LogMessage>;

template<>
struct std::less<LogMessage> {
    bool operator()(const LogMessage& a, const LogMessage& b) const {
        return a.time > b.time;
    }
};

namespace {
    // log sinks
    std::vector<log::ISink*> sinks = {};

    /// should we use the log thread queue?, if not we just send directly to all sinks
    /// this is set to false during shutdown as the the log thread will be closed before
    /// all messages we need to send are sent
    std::atomic_bool gEnableLogQueue = true;
    std::atomic<log::MessageTime> gLastMessageTime;
    threads::ThreadHandle *gLogThread = nullptr;

    LogQueue *getLogQueue() {
        static LogQueue *pQueue = new LogQueue(cfgLogQueueSize.getCurrentValue());
        return pQueue;
    }

    void addMessageToQueue(LogMessage&& msg) {
        auto *pQueue = getLogQueue();
        gLastMessageTime = msg.time;
        pQueue->enqueue(std::move(msg));
    }

    void sendMessageToSinks(const LogMessage& msg) {
        for (log::ISink *sink : sinks) {
            sink->addLogMessage(msg.level, msg.id, msg.time, msg.msg);
        }
    }
}

LoggingService::LoggingService() {
    addSink(new log::ConsoleSink());
    addSink(new log::FileSink());
}

bool LoggingService::createService() {
    gLogThread = ThreadService::newThread(threads::eBackground, "logger", [pQueue = getLogQueue()](std::stop_token token) {
        auto interval = std::chrono::milliseconds(cfgLogQueueInterval.getCurrentValue());

        std::priority_queue<LogMessage> pendingMessages;

        std::array<LogMessage, 32> messages;
        while (!token.stop_requested()) {
            // TODO: clean this all up
            size_t got = pQueue->tryGetBulk(messages.data(), messages.size(), interval);
            for (size_t i = 0; i < got; ++i) {
                pendingMessages.push(std::move(messages[i]));
            }

            // send all messages that are ready
            while (!pendingMessages.empty()) {
                auto& msg = pendingMessages.top();
                sendMessageToSinks(msg);
                pendingMessages.pop();
            }
        }
    });

    LOG_INFO("log level: {}", log::toString(cfgLogLevel.getCurrentValue()));
    return true;
}

void LoggingService::destroyService() {
    gEnableLogQueue = false;
    
    // pump the rest of the messages
    std::array<LogMessage, 32> messages;
    size_t got = getLogQueue()->tryGetBulk(messages.data(), messages.size(), std::chrono::milliseconds(1));
    for (size_t i = 0; i < got; ++i) {
        sendMessageToSinks(messages[i]);
    }
}

// public interface

bool LoggingService::shouldSend(log::Level level) {
    return cfgLogLevel.getCurrentValue() >= level;
}

void LoggingService::addSink(log::ISink *pSink) {
    sinks.push_back(pSink);
}

// private interface

void LoggingService::sendMessageAlways(log::Level msgLevel, std::string msg) {
    auto threadId = ThreadService::getCurrentThreadId();
    auto currentUtcTime = chrono::utc_clock::to_sys(chrono::utc_clock::now());

    LogMessage message = {
        .level = msgLevel,
        .id = threadId,
        .time = currentUtcTime,
        .msg = std::move(msg),
    };

    addMessageToQueue(std::move(message));
}

void LoggingService::throwAssert(std::string msg) {
    sendMessageAlways(log::eAssert, msg);
    core::throwFatal(msg);
}
