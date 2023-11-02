#pragma once

#include "engine/core/mt.h"

#include "engine/service/platform.h"

#include "engine/threads/thread.h"

#include <array>
#include <vector>
#include <format>

namespace simcoe {
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

        std::string_view name;
        threads::ThreadId threadId;
        std::chrono::system_clock::time_point time;

        std::string_view msg;
    };

    namespace logging {
        std::string formatMessage(const LogMessage& msg);
        std::string formatMessageColour(const LogMessage& msg);
    }

    struct ISink {
        virtual ~ISink() = default;

        virtual void accept(const LogMessage& msg) = 0;
    };

    struct ConsoleSink : ISink {
        void accept(const LogMessage& msg) override;

    private:
        std::mutex lock;
    };

    struct LoggingService final : IStaticService<LoggingService> {
        LoggingService();

        // IStaticService
        static constexpr std::string_view kServiceName = "logging";
        static constexpr std::array kServiceDeps = { PlatformService::kServiceName };

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

#define COLOUR_RED "\x1B[1;31m"    ///< ANSI escape string for red
#define COLOUR_GREEN "\x1B[1;32m"  ///< ANSI escape string for green
#define COLOUR_YELLOW "\x1B[1;33m" ///< ANSI escape string for yellow
#define COLOUR_BLUE "\x1B[1;34m"   ///< ANSI escape string for blue
#define COLOUR_PURPLE "\x1B[1;35m" ///< ANSI escape string for purple
#define COLOUR_CYAN "\x1B[1;36m"   ///< ANSI escape string for cyan
#define COLOUR_RESET "\x1B[0m"     ///< ANSI escape reset
