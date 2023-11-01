#pragma once

#include "engine/service/platform.h"

#include "engine/threads/thread.h"

namespace simcoe {
    // collects thread geometry at startup
    struct ThreadService : IStaticService<ThreadService> {
        // IStaticService
        static constexpr std::string_view kServiceName = "threads";
        static constexpr std::array kServiceDeps = { PlatformService::kServiceName };

        // IService
        bool createService() override;
        void destroyService() override;

        // failure reason
        static std::string_view getFailureReason();

        // geometry data
        static const threads::Geometry& getGeometry();

        // os thread functions
        static void migrateCurrentThread(const threads::Subcore& subcore);
        static threads::ThreadId getCurrentThreadId();

        // scheduler

    private:
        std::string_view failureReason = "";

        threads::Geometry geometry = {};
    };
}
