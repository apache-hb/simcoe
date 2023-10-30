#pragma once

#include "engine/service/platform.h"

#include "engine/threads/thread.h"

namespace simcoe {
    enum ThreadType {
        eThreadPerformance,
        eThreadEfficiency,

        eThreadCount
    };

    struct LogicalThread {
        size_t schedule; ///< the threads schedule speed (higher is faster)
        ThreadType type; ///< the threads type (performance or efficiency)

        KAFFINITY affinity; ///< the threads affinity mask
        WORD group; ///< the threads group
    };

    struct PhysicalCore {
        std::vector<size_t> threadIds;
    };

    // equivalent to a ryzen ccx or ccd
    struct CoreCluster {
        std::vector<size_t> coreIds;
    };

    // a single cpu package
    struct Package {
        std::vector<CoreCluster> clusters;
        std::vector<PhysicalCore> cores;
        std::vector<LogicalThread> threads;
    };

    // the cpu geometry
    struct Geometry {
        std::vector<Package> packages;
    };

    struct ThreadService : IStaticService<ThreadService> {
        // IStaticService
        static constexpr std::string_view kServiceName = "threads";
        static constexpr std::array kServiceDeps = { PlatformService::kServiceName };

        // IService
        bool createService() override;
        void destroyService() override;

        static threads::ThreadId getCurrentThreadId();

    private:
        Geometry geometry = {};
    };
}
