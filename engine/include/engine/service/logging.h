#pragma once

#include "engine/service/service.h"

#include <array>

namespace simcoe {
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
        static void logInfo(std::string_view msg, A&&... args) {
            USE_SERVICE(logInfo)(std::vformat(msg, std::make_format_args(args...)));
        }

        template<typename... A>
        static void logWarn(std::string_view msg, A&&... args) {
            USE_SERVICE(logWarn)(std::vformat(msg, std::make_format_args(args...)));
        }

        template<typename... A>
        static void logError(std::string_view msg, A&&... args) {
            USE_SERVICE(logError)(std::vformat(msg, std::make_format_args(args...)));
        }

        template<typename... A>
        static void logAssert(std::string_view msg, A&&... args) {
            USE_SERVICE(logAssert)(std::vformat(msg, std::make_format_args(args...)));
        }

    private:
        void logInfo(std::string_view msg);
        void logWarn(std::string_view msg);
        void logError(std::string_view msg);
        void logAssert(std::string_view msg);
    };
}
