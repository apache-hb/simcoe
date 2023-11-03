#pragma once

#include "engine/core/mt.h"

#include "engine/service/platform.h"

#include "engine/threads/thread.h"

#include <array>
#include <vector>
#include <format>

namespace simcoe {
    using MessageTime = std::chrono::system_clock::time_point;

    enum LogLevel {
        eAssert,
        eError,
        eWarn,
        eInfo,
        eDebug,

        eTotal
    };

    struct LogMessage {
        LogLevel level;

        threads::ThreadId threadId;
        MessageTime time;

        std::string_view msg;
    };

    namespace logging {
        std::string formatMessage(const LogMessage& msg);
        std::string formatMessageColour(const LogMessage& msg);
    }

    struct ISink {
        virtual ~ISink() = default;

        ISink(bool bSplitLines = false)
            : bSplitLines(bSplitLines)
        { }

        virtual void accept(const LogMessage& msg) = 0;

        void addLogMessage(LogLevel level, threads::ThreadId threadId, MessageTime time, std::string_view msg);

    private:
        // should each line be sent as as seperate message?
        bool bSplitLines;
    };

    struct StreamSink final : ISink {
        StreamSink(std::ostream& os)
            : ISink(true)
            , os(os)
        { }

        void accept(const LogMessage& msg) override;

    private:
        std::mutex mutex;
        std::ostream& os;
    };

    struct LoggingService final : IStaticService<LoggingService> {
        LoggingService();

        // IStaticService
        static constexpr std::string_view kServiceName = "logging";
        static constexpr std::array kServiceDeps = { PlatformService::kServiceName };
        static const config::ISchemaBase *gConfigSchema;

        // IService
        bool createService() override;
        void destroyService() override;

        // LoggingService
        template<typename... A>
        static void logDebug(std::string_view msg, A&&... args) {
            if (!shouldSend(eDebug)) { return; }

            get()->sendMessage(eDebug, std::vformat(msg, std::make_format_args(args...)));
        }

        template<typename... A>
        static void logInfo(std::string_view msg, A&&... args) {
            if (!shouldSend(eInfo)) { return; }

            get()->sendMessage(eInfo, std::vformat(msg, std::make_format_args(args...)));
        }

        template<typename... A>
        static void logWarn(std::string_view msg, A&&... args) {
            if (!shouldSend(eWarn)) { return; }

            get()->sendMessage(eWarn, std::vformat(msg, std::make_format_args(args...)));
        }

        template<typename... A>
        static void logError(std::string_view msg, A&&... args) {
            if (!shouldSend(eError)) { return; }

            get()->sendMessage(eError, std::vformat(msg, std::make_format_args(args...)));
        }

        template<typename... A>
        static void logAssert(std::string_view msg, A&&... args) {
            // we always send asserts
            get()->throwAssert(std::vformat(msg, std::make_format_args(args...)));
        }

        static bool shouldSend(LogLevel level) { return get()->level >= level; }

        template<std::derived_from<ISink> T, typename... A> requires std::constructible_from<T, A...>
        static T *newSink(A&&... args) {
            auto pSink = new T(std::forward<A>(args)...);
            addSink(pSink);
            return pSink;
        }

        static void addSink(ISink *pSink) {
            get()->addLogSink(pSink);
        }

    private:
        void sendMessage(LogLevel msgLevel, std::string_view msg);
        void throwAssert(std::string_view msg);

        // log filtering
        LogLevel level = eInfo;

        // sinks
        void addLogSink(ISink *pSink);

        mt::shared_mutex lock;
        std::vector<ISink*> sinks;
    };
}

#define LOG_DEBUG(...) simcoe::LoggingService::logDebug(__VA_ARGS__)
#define LOG_INFO(...) simcoe::LoggingService::logInfo(__VA_ARGS__)
#define LOG_WARN(...) simcoe::LoggingService::logWarn(__VA_ARGS__)
#define LOG_ERROR(...) simcoe::LoggingService::logError(__VA_ARGS__)
#define LOG_ASSERT(...) simcoe::LoggingService::logAssert(__VA_ARGS__)
