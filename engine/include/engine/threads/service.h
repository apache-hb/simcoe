#pragma once

#include "engine/service/platform.h"

#include "engine/threads/thread.h"

namespace simcoe {
    struct ThreadMask {
        GROUP_AFFINITY affinity; ///< the threads affinity mask
    };

    struct LogicalThread {
        ThreadMask mask;
    };

    // a single core, may have multiple threads due to hyperthreading
    struct PhysicalCore {
        uint16_t schedule; ///< the threads schedule speed (higher is faster)
        uint8_t efficiency; ///< the threads efficiency (higher is more efficient)

        ThreadMask mask;
        std::vector<uint16_t> threadIds;
    };

    // equivalent to a ryzen ccx or ccd
    struct CoreCluster {
        ThreadMask mask;
        std::vector<uint16_t> coreIds;
    };

    // a single cpu package
    struct Package {
        ThreadMask mask;

        std::vector<uint16_t> cores;
        std::vector<uint16_t> threads;
        std::vector<uint16_t> clusters;
    };

    // the cpu geometry
    struct Geometry {
        std::vector<LogicalThread> threads;
        std::vector<PhysicalCore> cores;
        std::vector<CoreCluster> clusters;
        std::vector<Package> packages;
    };

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

        // optional stuff
        static Geometry getGeometry();
        static void migrateCurrentThread(const LogicalThread& thread);

    private:
        std::string_view failureReason = "";
        Geometry geometry = {};
    };
}
