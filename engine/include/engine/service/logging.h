#pragma once

#include "engine/service/service.h"

#include <array>

namespace simcoe {
    enum LogLevel {
        eDebug,
        eInfo,
        eWarn,
        eError,
        eAssert,

        eTotal
    };

    struct ISink {
        virtual ~ISink() = default;
    };

    struct LoggingService final : IStaticService<LoggingService> {
        // IStaticService
        static constexpr std::string_view kServiceName = "logging";
        static constexpr std::array<std::string_view, 0> kServiceDeps = { };

        // IService
        void createService() override;
        void destroyService() override;

        // LoggingService
        template<typename... A>
        static void logDebug(std::string_view msg, A&&... args) {
            USE_SERVICE(sendMessage)(eDebug, std::vformat(msg, std::make_format_args(args...)));
        }

        template<typename... A>
        static void logInfo(std::string_view msg, A&&... args) {
            USE_SERVICE(sendMessage)(eInfo, std::vformat(msg, std::make_format_args(args...)));
        }

        template<typename... A>
        static void logWarn(std::string_view msg, A&&... args) {
            USE_SERVICE(sendMessage)(eWarn, std::vformat(msg, std::make_format_args(args...)));
        }

        template<typename... A>
        static void logError(std::string_view msg, A&&... args) {
            USE_SERVICE(sendMessage)(eError, std::vformat(msg, std::make_format_args(args...)));
        }

        template<typename... A>
        static void logAssert(std::string_view msg, A&&... args) {
            USE_SERVICE(throwAssert)(std::vformat(msg, std::make_format_args(args...)));
        }

    private:
        void sendMessage(LogLevel level, std::string_view msg);
        void throwAssert(std::string_view msg);
    };
}

#define LOG_DEBUG(...) simcoe::LoggingService::logDebug(__VA_ARGS__)
#define LOG_INFO(...) simcoe::LoggingService::logInfo(__VA_ARGS__)
#define LOG_WARN(...) simcoe::LoggingService::logWarn(__VA_ARGS__)
#define LOG_ERROR(...) simcoe::LoggingService::logError(__VA_ARGS__)
#define LOG_ASSERT(...) simcoe::LoggingService::logAssert(__VA_ARGS__)
