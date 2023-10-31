#pragma once

#include <thread>
#include <functional>

#include "engine/core/macros.h"
#include "engine/core/win32.h"

namespace simcoe::threads {
    using ThreadId = std::thread::id;
    using ThreadStart = std::function<void()>;

    struct ThreadMask {
        GROUP_AFFINITY affinity; ///< the threads affinity mask
    };

    struct LogicalThread {
        ThreadMask mask;
    };

    // a single core, may have multiple threads due to hyperthreading
    struct PhysicalCore {
        uint16_t schedule; ///< the threads schedule speed (lower is faster)
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

    struct Thread {
        // create a thread on a specific logical thread
        Thread(const LogicalThread& thread);

        // create a thread on a specific physical core (picks a thread inside it)
        Thread(const PhysicalCore& core);

        // create a thread on a specific core cluster (picks a thread inside it)
        Thread(const CoreCluster& cluster);

        ~Thread();

    private:
        HANDLE hThread;
    };
}
