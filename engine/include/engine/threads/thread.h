#pragma once

#include <functional>
#include <string_view>
#include <stop_token>
#include <format>

#include "engine/core/macros.h"
#include "engine/core/win32.h"

namespace simcoe { struct ThreadService; }

namespace simcoe::threads {
    using ThreadId = DWORD;
    using ThreadStart = std::function<void(std::stop_token)>;

    enum struct SubcoreIndex : uint16_t { eInvalid = UINT16_MAX };
    enum struct CoreIndex : uint16_t { eInvalid = UINT16_MAX };
    enum struct ChipletIndex : uint16_t { eInvalid = UINT16_MAX };
    enum struct PackageIndex : uint16_t { eInvalid = UINT16_MAX };

    using SubcoreIndices = std::vector<SubcoreIndex>;
    using CoreIndices = std::vector<CoreIndex>;
    using ChipletIndices = std::vector<ChipletIndex>;
    using PackageIndices = std::vector<PackageIndex>;

    struct ScheduleMask : GROUP_AFFINITY {
        using GROUP_AFFINITY::Mask;
        using GROUP_AFFINITY::Group;
    };

    struct LogicalThread {
        ScheduleMask mask;
    };

    using Subcore = LogicalThread;

    // a single core, may have multiple threads due to hyperthreading
    struct Core {
        uint16_t schedule; ///< the threads schedule speed (lower is faster)
        uint8_t efficiency; ///< the threads efficiency (higher is more efficient)

        ScheduleMask mask;
        SubcoreIndices subcoreIds;
    };

    // equivalent to a ryzen ccx or ccd
    struct Chiplet {
        ScheduleMask mask;
        CoreIndices coreIds;
    };

    // a single cpu package
    struct Package {
        ScheduleMask mask;

        CoreIndices cores;
        SubcoreIndices subcores;
        ChipletIndices chiplets;
    };

    // the cpu geometry
    struct Geometry {
        std::vector<Subcore> subcores;
        std::vector<Core> cores;
        std::vector<Chiplet> chiplets;
        std::vector<Package> packages;

        const Subcore& getSubcore(SubcoreIndex idx) const { return subcores[size_t(idx)]; }
        const Core& getCore(CoreIndex idx) const { return cores[size_t(idx)]; }
        const Chiplet& getChiplet(ChipletIndex idx) const { return chiplets[size_t(idx)]; }
        const Package& getPackage(PackageIndex idx) const { return packages[size_t(idx)]; }
    };

    enum ThreadType {
        // a thread that needs to be realtime, or almost realtime
        // e.g. a thread that processes audio data
        eRealtime,

        // a thread that needs to be responsive, but not realtime
        // e.g. a thread that processes user input or game logic
        eResponsive,

        // a thread that doesnt need meet any timing requirements
        // e.g. a thread that writes to a log file or processes network data
        eBackground,

        // long running thread that only needs to work occasionally
        // e.g. a thread that polls the system for performance data 1x per second
        eWorker,

        // total enum count
        eCount
    };

    struct ThreadInfo {
        ThreadType type;
        ScheduleMask mask;
        std::string_view name;
        ThreadStart start;
    };

    struct ThreadHandle {
        SM_NOCOPY(ThreadHandle)
        SM_NOMOVE(ThreadHandle)

        friend struct simcoe::ThreadService;

        std::string_view getName() const { return name; }
        HANDLE getHandle() const { return hThread; }
        ThreadId getId() const { return id; }
        ThreadType getType() const { return type; }
        ScheduleMask getAffinity() const { return mask; }

        void requestStop() { stopper.request_stop(); }

        ~ThreadHandle();

    private:
        ThreadHandle(ThreadInfo&& info);

        // starter thunk
        static DWORD WINAPI threadThunk(LPVOID lpParameter);

        // os data
        HANDLE hThread = nullptr;
        ThreadId id = 0;

        // schedule data
        ThreadType type;
        ScheduleMask mask;

        // thread data
        std::string_view name;
        std::stop_source stopper = {};
    };
}

SM_ENUM_INT(simcoe::threads::SubcoreIndex, SubcoreIndexUnderlying)
SM_ENUM_INT(simcoe::threads::CoreIndex, CoreIndexUnderlying)
SM_ENUM_INT(simcoe::threads::ChipletIndex, ChipletIndexUnderlying)
SM_ENUM_INT(simcoe::threads::PackageIndex, PackageIndexUnderlying)

SM_ENUM_FORMATTER(simcoe::threads::SubcoreIndex, SubcoreIndexUnderlying)
SM_ENUM_FORMATTER(simcoe::threads::CoreIndex, CoreIndexUnderlying)
SM_ENUM_FORMATTER(simcoe::threads::ChipletIndex, ChipletIndexUnderlying)
SM_ENUM_FORMATTER(simcoe::threads::PackageIndex, PackageIndexUnderlying)
