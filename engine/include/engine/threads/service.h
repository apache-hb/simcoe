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

        // stuff that works when failed
        static threads::ThreadId getCurrentThreadId();

        // geometry management
        static threads::Geometry getGeometry();

        // thread migration
        static void migrateCurrentThread(const threads::LogicalThread& thread);

        // TODO: thread stats (how many threads are running, on which cores, etc)

    private:
        std::string_view failureReason = "";

        threads::Geometry geometry = {};
    };
}
