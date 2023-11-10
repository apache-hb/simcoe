#pragma once

#include "engine/core/mt.h"

#include "engine/service/service.h"
#include "engine/service/platform.h"

#include "engine/log/log.h"

#include <array>

namespace simcoe {
    struct LoggingService final : IStaticService<LoggingService> {
        LoggingService();

        // IStaticService
        static constexpr std::string_view kServiceName = "logging";
        static inline auto kServiceDeps = depends(ConfigService::service(), PlatformService::service());

        // IService
        bool createService() override;
        void destroyService() override;

        // LoggingService
        template<typename... A>
        static void logDebug(std::string_view msg, A&&... args) {
            if (!shouldSend(log::eDebug)) { return; }

            sendMessageAlways(log::eDebug, std::vformat(msg, std::make_format_args(args...)));
        }

        template<typename... A>
        static void logInfo(std::string_view msg, A&&... args) {
            if (!shouldSend(log::eInfo)) { return; }

            sendMessageAlways(log::eInfo, std::vformat(msg, std::make_format_args(args...)));
        }

        template<typename... A>
        static void logWarn(std::string_view msg, A&&... args) {
            if (!shouldSend(log::eWarn)) { return; }

            sendMessageAlways(log::eWarn, std::vformat(msg, std::make_format_args(args...)));
        }

        template<typename... A>
        static void logError(std::string_view msg, A&&... args) {
            if (!shouldSend(log::eError)) { return; }

            sendMessageAlways(log::eError, std::vformat(msg, std::make_format_args(args...)));
        }

        template<typename... A>
        static void logAssert(std::string_view msg, A&&... args) {
            // we always send asserts
            throwAssert(std::vformat(msg, std::make_format_args(args...)));
        }

        static bool sendMessage(log::Level msgLevel, std::string_view msg) {
            if (!shouldSend(msgLevel)) { return false; }

            sendMessageAlways(msgLevel, msg);
            return true;
        }

        static bool shouldSend(log::Level level);
        static void addSink(std::string_view name, log::ISink *pSink);

    private:
        static void sendMessageAlways(log::Level msgLevel, std::string_view msg);
        static void throwAssert(std::string msg);
    };
}

#define LOG_DEBUG(...) simcoe::LoggingService::logDebug(__VA_ARGS__)
#define LOG_INFO(...) simcoe::LoggingService::logInfo(__VA_ARGS__)
#define LOG_WARN(...) simcoe::LoggingService::logWarn(__VA_ARGS__)
#define LOG_ERROR(...) simcoe::LoggingService::logError(__VA_ARGS__)
#define LOG_ASSERT(...) simcoe::LoggingService::logAssert(__VA_ARGS__)
