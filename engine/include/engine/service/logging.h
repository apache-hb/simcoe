#pragma once

#include "engine/service/debug.h"

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

    struct ISink {
        virtual ~ISink() = default;

        virtual void accept(std::string_view msg) = 0;
    };

    struct ConsoleSink : ISink {
        void accept(std::string_view msg) override;
    };

    struct LoggingService final : IStaticService<LoggingService> {
        // IStaticService
        static constexpr std::string_view kServiceName = "logging";
        static constexpr std::array<std::string_view, 0> kServiceDeps = { DebugService::kServiceName };

        // IService
        void createService() override;
        void destroyService() override;

        // LoggingService
        template<typename... A>
        static void logDebug(std::string_view msg, A&&... args) {
            if (!shouldSend(eDebug)) { return; }

            USE_SERVICE(sendMessage)(eDebug, std::vformat(msg, std::make_format_args(args...)));
        }

        template<typename... A>
        static void logInfo(std::string_view msg, A&&... args) {
            if (!shouldSend(eInfo)) { return; }

            USE_SERVICE(sendMessage)(eInfo, std::vformat(msg, std::make_format_args(args...)));
        }

        template<typename... A>
        static void logWarn(std::string_view msg, A&&... args) {
            if (!shouldSend(eWarn)) { return; }

            USE_SERVICE(sendMessage)(eWarn, std::vformat(msg, std::make_format_args(args...)));
        }

        template<typename... A>
        static void logError(std::string_view msg, A&&... args) {
            if (!shouldSend(eError)) { return; }

            USE_SERVICE(sendMessage)(eError, std::vformat(msg, std::make_format_args(args...)));
        }

        template<typename... A>
        static void logAssert(std::string_view msg, A&&... args) {
            // we always send asserts
            USE_SERVICE(throwAssert)(std::vformat(msg, std::make_format_args(args...)));
        }

        static void setLevel(LogLevel level) { get()->minLevel = level; }
        static bool shouldSend(LogLevel level) { return get()->minLevel <= level; }

        template<std::derived_from<ISink> T, typename... A> requires std::constructible_from<T, A...>
        static T *newSink(A&&... args) {
            auto pSink = new T(std::forward<A>(args)...);
            get()->addLogSink(pSink);
            return pSink;
        }

    private:
        void sendMessage(LogLevel level, std::string_view msg);
        void throwAssert(std::string_view msg);

        // log filtering
        std::atomic<LogLevel> minLevel = eInfo;

        // sinks
        void addLogSink(ISink *pSink);

        std::mutex lock;
        std::vector<ISink*> sinks;
    };
}

#define LOG_DEBUG(...) simcoe::LoggingService::logDebug(__VA_ARGS__)
#define LOG_INFO(...) simcoe::LoggingService::logInfo(__VA_ARGS__)
#define LOG_WARN(...) simcoe::LoggingService::logWarn(__VA_ARGS__)
#define LOG_ERROR(...) simcoe::LoggingService::logError(__VA_ARGS__)
#define LOG_ASSERT(...) simcoe::LoggingService::logAssert(__VA_ARGS__)
