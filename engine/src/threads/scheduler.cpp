#include "engine/threads/schedule.h"

#include "engine/service/logging.h"

#include "engine/core/range.h"

#include <numeric>

using namespace simcoe;
using namespace simcoe::threads;

namespace {
    struct CorePerf {
        CoreIndex index;
        uint16_t schedule;
    };

    struct ChipletPerf {
        ChipletIndex index;
        uint16_t score;
    };

    struct PackagePerf {
        PackageIndex index;
        uint16_t score;
    };

    static constexpr auto kThreadCosts = std::to_array<uint16_t>({
        /*eRealtime*/   UINT16_MAX,
        /*eResponsive*/ 100,
        /*eBackground*/ 50,
        /*eWorker*/     5
    });
}

Scheduler::Scheduler() {
    setupThreadData();
}

Scheduler::~Scheduler() {
    std::lock_guard guard(lock);

    // TODO: handle threads being destroyed while the scheduler is still running
    threads.clear();
}

Thread& Scheduler::newThread(ThreadType type, std::string_view name, ThreadStart&& start) {
    std::lock_guard guard(lock);
    const auto& geometry = ThreadService::getGeometry();
    SubcoreIndex index = getBestSubcore(type, name);
    LOG_INFO("new thread {} on thread {} with load {}", name, index, subcoreUsage[index]);

    subcoreUsage[index] += kThreadCosts[size_t(type)];
    threads.emplace_back(geometry.getSubcore(index), name, std::move(start));
    return threads.back();
}

uint16_t Scheduler::getCoreLoad(CoreIndex coreIdx) const {
    const auto& geometry = ThreadService::getGeometry();

    uint16_t load = 0;

    for (SubcoreIndex subcoreId : geometry.getCore(coreIdx).subcoreIds) {
        uint16_t threadLoad = subcoreUsage.contains(subcoreId) ? subcoreUsage.at(subcoreId) : 0;
        if (threadLoad == UINT16_MAX) return UINT16_MAX;

        load += threadLoad;
    }

    return load;
}

SubcoreIndex Scheduler::getLeastLoadedSubcore(CoreIndex coreIdx) const {
    const auto& geometry = ThreadService::getGeometry();

    uint16_t bestLoad = UINT16_MAX;
    SubcoreIndex bestSubcore = SubcoreIndex::eInvalid;

    for (SubcoreIndex subcoreId : geometry.getCore(coreIdx).subcoreIds) {
        uint16_t threadLoad = subcoreUsage.contains(subcoreId) ? subcoreUsage.at(subcoreId) : 0;
        if (threadLoad < bestLoad) {
            bestLoad = threadLoad;
            bestSubcore = subcoreId;
        }
    }

    if (bestLoad == UINT16_MAX) {
        LOG_ERROR("attempting to allocate a new thread on core {} but all subcores are at max load, your computer cries for help :(", coreIdx);
    }

    return bestSubcore;
}

SubcoreIndex Scheduler::getBestSubcore(ThreadType type, std::string_view name) const {
    if (type == eRealtime) {
        // find the fastest core that has nothing running on it
        for (CoreIndex coreIdx : coreRanking) {
            uint16_t load = getCoreLoad(coreIdx);
            if (load < 0) continue;

            return getLeastLoadedSubcore(coreIdx);
        }

        LOG_WARN("no free cores available for realtime thread, finding the least loaded core");

        // TODO: implement a way to shove threads off their current cores to make room

        // find the fastest core that has the least amount of load
        CoreIndex bestCore = coreRanking[0];
        uint16_t bestLoad = getCoreLoad(bestCore);

        for (CoreIndex coreIdx : coreRanking) {
            uint16_t load = getCoreLoad(coreIdx);
            if (load < bestLoad) {
                bestCore = coreIdx;
                bestLoad = load;
            }
        }

        LOG_WARN("realtime thread allocated to core {}", bestCore);
        return getLeastLoadedSubcore(bestCore);
    }

    // nothing else can run on a core with a realtime thread

    if (type == eResponsive) {
        // find the fastest core that doesnt have a realtime thread on it, can have one other thread running on it
        CoreIndex bestCore = coreRanking[0];
        uint16_t bestLoad = getCoreLoad(bestCore);

        for (CoreIndex coreIdx : coreRanking) {
            uint16_t load = getCoreLoad(coreIdx);
            if (load < bestLoad) {
                bestCore = coreIdx;
                bestLoad = load;
            }
        }

        LOG_INFO("responsive thread allocated to core {}", bestCore);

        return getLeastLoadedSubcore(bestCore);
    }

    const auto& geometry = ThreadService::getGeometry();

    // these threads can go anywhere
    if (type == eBackground) {
        // find a core that has a low performance
        // prefer efficient cores over inefficient ones

        CoreIndex bestCore = coreRanking[0];
        uint16_t bestEfficiency = geometry.getCore(bestCore).efficiency;
        for (CoreIndex coreIdx : coreRanking) {
            uint16_t efficiency = geometry.getCore(coreIdx).efficiency;
            if (efficiency > bestEfficiency) {
                bestCore = coreIdx;
                bestEfficiency = efficiency;
            }
        }
    }

    if (type == eWorker) {
        // these threads should go on the slowest cores
        // prefer efficient cores over inefficient ones

        CoreIndex bestCore = coreRanking[0];
        uint16_t bestEfficiency = geometry.getCore(bestCore).efficiency;
        uint16_t worstSpeed = geometry.getCore(bestCore).schedule;

        for (CoreIndex coreIdx : coreRanking) {
            uint16_t efficiency = geometry.getCore(coreIdx).efficiency;
            if (efficiency > bestEfficiency) {
                bestCore = coreIdx;
                bestEfficiency = efficiency;
            } else if (efficiency == bestEfficiency) {
                uint16_t speed = geometry.getCore(coreIdx).schedule;
                if (speed > worstSpeed) {
                    bestCore = coreIdx;
                    worstSpeed = speed;
                }
            }
        }

        return getLeastLoadedSubcore(bestCore);
    }

    LOG_ERROR("unknown thread type {}", int(type));
    return SubcoreIndex::eInvalid;
}

void Scheduler::setupThreadData() {
    const auto& geometry = ThreadService::getGeometry();

    std::vector<CorePerf> corePerfs;
    for (const auto& [index, core] : core::enumerate<CoreIndex>(geometry.cores)) {
        corePerfs.push_back({ .index = index, .schedule = core.schedule });
    }

    std::vector<ChipletPerf> chipletPerfs;
    for (const auto& [index, cluster] : core::enumerate<ChipletIndex>(geometry.chiplets)) {
        uint16_t score = 0;
        for (CoreIndex core : cluster.coreIds) {
            score += geometry.getCore(core).schedule;
        }

        chipletPerfs.push_back({ .index = index, .score = score });
    }

    std::vector<PackagePerf> packPerfs;
    for (const auto& [index, package] : core::enumerate<PackageIndex>(geometry.packages)) {
        uint16_t score = 0;
        for (ChipletIndex chiplet : package.chiplets) {
            score += chipletPerfs[size_t(chiplet)].score;
        }

        packPerfs.push_back({ .index = index, .score = score });
    }

    std::sort(corePerfs.begin(), corePerfs.end(), [](const auto& a, const auto& b) {
        return a.schedule < b.schedule;
    });

    std::sort(chipletPerfs.begin(), chipletPerfs.end(), [](const auto& a, const auto& b) {
        return a.score < b.score;
    });

    std::sort(packPerfs.begin(), packPerfs.end(), [](const auto& a, const auto& b) {
        return a.score < b.score;
    });

    std::transform(
        corePerfs.begin(), corePerfs.end(),
        std::back_inserter(coreRanking),
        [](const auto& perf) { return perf.index; }
    );

    std::transform(
        chipletPerfs.begin(), chipletPerfs.end(),
        std::back_inserter(chipletRanking),
        [](const auto& perf) { return perf.index; }
    );

    std::transform(
        packPerfs.begin(), packPerfs.end(),
        std::back_inserter(packageRanking),
        [](const auto& perf) { return perf.index; }
    );

    LOG_INFO("collated thread performance data");
}
