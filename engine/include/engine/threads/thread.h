#pragma once

#include <functional>
#include <string_view>
#include <stop_token>

#include "engine/core/macros.h"
#include "engine/core/win32.h"

namespace simcoe::threads {
    using ThreadId = DWORD;
    using ThreadStart = std::function<void(std::stop_token)>;

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
        SM_NOCOPY(Thread)

        Thread(Thread&&) noexcept;
        Thread& operator=(Thread&&) noexcept;

        template<std::invocable<std::stop_token> T>
        Thread(T&& start) : Thread(ThreadStart(start)) { }

        // create a thread
        Thread(ThreadStart&& start);

        // create a thread on a specific logical thread
        Thread(const LogicalThread& thread, ThreadStart&& start);

        // create a thread on a specific physical core (picks a thread inside it)
        Thread(const PhysicalCore& core, ThreadStart&& start);

        // create a thread on a specific core cluster (picks a thread inside it)
        Thread(const CoreCluster& cluster, ThreadStart&& start);

        ~Thread();

        const LogicalThread& getThreadIdx() const { return threadIdx; }
        HANDLE getHandle() const { return hThread; }
        ThreadId getId() const { return id; }

    private:
        static DWORD WINAPI threadThunk(LPVOID lpParameter);

        LogicalThread threadIdx = {};

        HANDLE hThread = nullptr;
        ThreadId id = 0;

        std::stop_source stopper = {};
    };
}
