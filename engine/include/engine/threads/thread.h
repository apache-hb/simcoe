#pragma once

#include <functional>
#include <string_view>
#include <stop_token>
#include <format>

#include "engine/core/macros.h"
#include "engine/core/win32.h"

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

    struct Thread {
        SM_NOCOPY(Thread)

        Thread(Thread&&) noexcept;
        Thread& operator=(Thread&&) noexcept;

        // create a thread on a specific subcore
        Thread(const Subcore& subcore, std::string_view name, ThreadStart&& start);

        ~Thread();

        const Subcore& getSubcore() const { return subcore; }
        HANDLE getHandle() const { return hThread; }
        ThreadId getId() const { return id; }

    private:
        static DWORD WINAPI threadThunk(LPVOID lpParameter);

        Subcore subcore = {};

        HANDLE hThread = nullptr;
        ThreadId id = 0;

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
