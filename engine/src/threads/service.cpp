#include "engine/threads/service.h"

#include "engine/core/error.h"
#include "engine/core/units.h"

#include "engine/config/system.h"

#include "engine/log/service.h"

#include <unordered_set>

using namespace simcoe;
using namespace simcoe::threads;

using namespace std::chrono_literals;

config::ConfigValue<size_t> cfgDefaultWorkerCount("threads/workers", "initial", "Default number of worker threads (0 = system default)", 0);
config::ConfigValue<size_t> cfgMaxWorkerCount("threads/workers", "max", "Maximum number of worker threads (0 = no limit)", 0);
config::ConfigValue<size_t> cfgWorkerDelay("threads/workers", "delay", "Delay between worker thread polls (in ms)", 50);

config::ConfigValue<size_t> cfgWorkQueueSize("threads", "workQueueSize", "Size of the work queue", 256);
config::ConfigValue<size_t> cfgMainQueueSize("threads", "mainQueueSize", "Size of the main queue", 64);

namespace {
    template<typename T>
    T *advance(T *ptr, size_t bytes) {
        return reinterpret_cast<T*>(reinterpret_cast<std::byte*>(ptr) + bytes);
    }

    struct ProcessorInfoIterator {
        ProcessorInfoIterator(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *pBuffer, DWORD remaining)
            : pBuffer(pBuffer)
            , remaining(remaining)
        { }

        SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *operator++() {
            SM_ASSERT(remaining > 0);

            remaining -= pBuffer->Size;
            if (remaining > 0) {
                pBuffer = advance(pBuffer, pBuffer->Size);
            }

            return pBuffer;
        }

        SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *operator*() const {
            return pBuffer;
        }

        bool operator==(const ProcessorInfoIterator& other) const {
            return remaining == other.remaining;
        }

    private:
        SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *pBuffer;
        DWORD remaining;
    };

    // GetLogicalProcessorInformationEx provides information about inter-core communication
    struct ProcessorInfo {
        ProcessorInfo(LOGICAL_PROCESSOR_RELATIONSHIP relation) {
            if (GetLogicalProcessorInformationEx(relation, nullptr, &bufferSize)) {
                core::throwNonFatal("GetLogicalProcessorInformationEx did not fail");
            }

            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
                debug::throwLastError("GetLogicalProcessorInformationEx did not fail with ERROR_INSUFFICIENT_BUFFER");
            }

            memory = std::make_unique<std::byte[]>(bufferSize);
            pBuffer = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(memory.get());
            if (!GetLogicalProcessorInformationEx(relation, pBuffer, &bufferSize)) {
                debug::throwLastError("GetLogicalProcessorInformationEx failed");
            }

            remaining = bufferSize;
        }

        ProcessorInfoIterator begin() const {
            return ProcessorInfoIterator(pBuffer, remaining);
        }

        ProcessorInfoIterator end() const {
            return ProcessorInfoIterator(pBuffer, 0);
        }

    private:
        std::unique_ptr<std::byte[]> memory;
        SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *pBuffer;
        DWORD bufferSize = 0;

        DWORD remaining = 0;
    };

    // GetSystemCpuSetInformation provides information about cpu speeds
    struct CpuSetIterator {
        CpuSetIterator(SYSTEM_CPU_SET_INFORMATION *pBuffer, ULONG remaining)
            : pBuffer(pBuffer)
            , remaining(remaining)
        { }

        SYSTEM_CPU_SET_INFORMATION *operator++() {
            SM_ASSERT(remaining > 0);

            remaining -= pBuffer->Size;
            if (remaining > 0) {
                pBuffer = advance(pBuffer, pBuffer->Size);
            }

            return pBuffer;
        }

        SYSTEM_CPU_SET_INFORMATION *operator*() const {
            return pBuffer;
        }

        bool operator==(const CpuSetIterator& other) const {
            return remaining == other.remaining;
        }

    private:
        SYSTEM_CPU_SET_INFORMATION *pBuffer;
        ULONG remaining;
    };

    struct CpuSetInfo {
        CpuSetInfo() {
            if (GetSystemCpuSetInformation(nullptr, 0, &bufferSize, GetCurrentProcess(), 0)) {
                core::throwNonFatal("GetSystemCpuSetInformation did not fail");
            }

            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
                debug::throwLastError("GetSystemCpuSetInformation did not fail with ERROR_INSUFFICIENT_BUFFER");
            }

            memory = std::make_unique<std::byte[]>(bufferSize);
            pBuffer = reinterpret_cast<SYSTEM_CPU_SET_INFORMATION *>(memory.get());
            if (!GetSystemCpuSetInformation(pBuffer, bufferSize, &bufferSize, GetCurrentProcess(), 0)) {
                debug::throwLastError("GetSystemCpuSetInformation failed");
            }

            remaining = bufferSize;
        }

        CpuSetIterator begin() const {
            return CpuSetIterator(pBuffer, remaining);
        }

        CpuSetIterator end() const {
            return CpuSetIterator(pBuffer, 0);
        }
    private:
        std::unique_ptr<std::byte[]> memory;
        SYSTEM_CPU_SET_INFORMATION *pBuffer;
        ULONG bufferSize = 0;

        ULONG remaining = 0;
    };

    struct GeometryBuilder {
        std::vector<Subcore> subcores;
        std::vector<Core> cores;
        std::vector<Chiplet> chiplets;
        std::vector<Package> packages;

        template<typename Index, typename Item>
        static void getItemByMask(std::vector<Index>& ids, std::span<const Item> items, GROUP_AFFINITY affinity) {
            std::unordered_set<Index> uniqueIds;
            for (size_t i = 0; i < items.size(); ++i) {
                const auto& item = items[i];
                if (item.mask.Group == affinity.Group && item.mask.Mask & affinity.Mask) {
                    uniqueIds.insert(core::enumCast<Index>(i));
                }
            }

            std::transform(uniqueIds.begin(), uniqueIds.end(), std::back_inserter(ids), [](Index id) {
                return id;
            });
        }

        void getCoresByMask(CoreIndices& ids, GROUP_AFFINITY affinity) const {
            getItemByMask<CoreIndex, Core>(ids, cores, affinity);
        }

        void getSubcoresByMask(SubcoreIndices& ids, GROUP_AFFINITY affinity) const {
            getItemByMask<SubcoreIndex, Subcore>(ids, subcores, affinity);
        }

        void getChipletsByMask(ChipletIndices& ids, GROUP_AFFINITY affinity) const {
            getItemByMask<ChipletIndex, Chiplet>(ids, chiplets, affinity);
        }
    };

    struct ProcessorInfoLayout {
        ProcessorInfoLayout(GeometryBuilder *pBuilder)
            : pBuilder(pBuilder)
        { }

        GeometryBuilder *pBuilder;
        size_t cacheCount = 0;

        static constexpr KAFFINITY KAFFINITY_BITS = std::numeric_limits<KAFFINITY>::digits;

        void addProcessorCore(const PROCESSOR_RELATIONSHIP *pInfo) {
            SubcoreIndices subcoreIds;

            for (DWORD i = 0; i < pInfo->GroupCount; ++i) {
                GROUP_AFFINITY group = pInfo->GroupMask[i];

                for (size_t bit = 0; bit < KAFFINITY_BITS; ++bit) {
                    KAFFINITY mask = 1ull << bit;
                    if (!(group.Mask & mask)) continue;

                    GROUP_AFFINITY groupAffinity = {
                        .Mask = group.Mask & mask,
                        .Group = group.Group,
                    };

                    pBuilder->subcores.push_back({
                        .mask = ScheduleMask(groupAffinity)
                    });

                    subcoreIds.push_back(core::enumCast<SubcoreIndex>(pBuilder->subcores.size() - 1));
                }
            }

            pBuilder->cores.push_back({
                .efficiency = pInfo->EfficiencyClass,
                .mask = ScheduleMask(pInfo->GroupMask[0]),
                .subcoreIds = subcoreIds
            });
        }

        void addProcessorPackage(const PROCESSOR_RELATIONSHIP *pInfo) {
            SubcoreIndices subcoreIds;
            CoreIndices coreIds;
            ChipletIndices chipletIds;

            for (DWORD i = 0; i < pInfo->GroupCount; ++i) {
                GROUP_AFFINITY group = pInfo->GroupMask[i];
                pBuilder->getCoresByMask(coreIds, group);
                pBuilder->getSubcoresByMask(subcoreIds, group);
                pBuilder->getChipletsByMask(chipletIds, group);
            }

            pBuilder->packages.push_back({
                .mask = ScheduleMask(pInfo->GroupMask[0]),
                .cores = coreIds,
                .subcores = subcoreIds,
                .chiplets = chipletIds
            });
        }

        void addCache(const CACHE_RELATIONSHIP& info) {
            if (info.Level != 3) return;

            // assume everything that shares l3 cache is in the same cluster
            // TODO: on intel E-cores and P-cores share the same l3
            //       but we dont want to put them in the same cluster.
            //       for now that isnt a big problem though.
            CoreIndices coreIds;

            for (DWORD i = 0; i < info.GroupCount; ++i) {
                GROUP_AFFINITY group = info.GroupMasks[i];
                pBuilder->getCoresByMask(coreIds, group);
            }

            pBuilder->chiplets.push_back({
                .mask = ScheduleMask(info.GroupMasks[0]),
                .coreIds = coreIds
            });
        }
    };

    struct CpuSetLayout {
        CpuSetLayout(GeometryBuilder *pBuilder)
            : pBuilder(pBuilder)
        { }

        GeometryBuilder *pBuilder;

        void addCpuSet(const SYSTEM_CPU_SET_INFORMATION *pInfo) {
            auto cpuSet = pInfo->CpuSet;
            GROUP_AFFINITY groupAffinity = {
                .Mask = 1ull << KAFFINITY(cpuSet.LogicalProcessorIndex),
                .Group = cpuSet.Group
            };

            CoreIndices coreIds;
            pBuilder->getCoresByMask(coreIds, groupAffinity);
            for (CoreIndex coreId : coreIds) {
                pBuilder->cores[size_t(coreId)].schedule = cpuSet.SchedulingClass;
            }
        }
    };

    // geometry data
    Geometry gCpuGeometry = {};

    // all currently scheduled threads
    mt::SharedMutex gThreadLock{"pool"};
    std::vector<threads::ThreadHandle*> gThreadHandles;

    // worker thread data
    size_t gWorkerId = 0;
    std::vector<threads::ThreadHandle*> gWorkers;

    // thread communication
    WorkQueue *gMainQueue = nullptr;
    BlockingWorkQueue *gWorkQueue = nullptr;
}

bool ThreadService::createService() {
    GeometryBuilder builder;
    ProcessorInfoLayout processorInfoLayout{&builder};
    CpuSetLayout cpuSetLayout{&builder};

    ProcessorInfo procInfo{RelationAll};

    for (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *pRelation : procInfo) {
        switch (pRelation->Relationship) {
        case RelationProcessorCore:
            processorInfoLayout.addProcessorCore(&pRelation->Processor);
            break;

        default:
            break;
        }
    }

    for (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *pRelation : procInfo) {
        switch (pRelation->Relationship) {
        case RelationCache:
            processorInfoLayout.addCache(pRelation->Cache);
            break;

        default:
            break;
        }
    }

    for (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *pRelation : procInfo) {
        switch (pRelation->Relationship) {
        case RelationProcessorPackage:
            processorInfoLayout.addProcessorPackage(&pRelation->Processor);
            break;

        default:
            break;
        }
    }

    CpuSetInfo cpuSetInfo;

    for (SYSTEM_CPU_SET_INFORMATION *pCpuSet : cpuSetInfo) {
        switch (pCpuSet->Type) {
        case CpuSetInformation:
            cpuSetLayout.addCpuSet(pCpuSet);
            break;

        default:
            break;
        }
    }

    LOG_INFO("CPU layout: (packages={} cores={} threads={} caches={})", builder.packages.size(), builder.cores.size(), builder.subcores.size(), processorInfoLayout.cacheCount);

    gCpuGeometry = {
        .subcores = builder.subcores,
        .cores = builder.cores,
        .chiplets = builder.chiplets,
        .packages = builder.packages
    };

    gMainQueue = new WorkQueue(cfgMainQueueSize.getCurrentValue());
    gWorkQueue = new BlockingWorkQueue(cfgWorkQueueSize.getCurrentValue());

    setWorkerCount(cfgDefaultWorkerCount.getCurrentValue());

    return true;
}

void ThreadService::destroyService() {
    ThreadService::shutdown();
}

const Geometry& ThreadService::getGeometry() {
    return gCpuGeometry;
}

ThreadId ThreadService::getCurrentThreadId() {
    return GetCurrentThreadId();
}

// main thread communication

void ThreadService::enqueueMain(std::string name, WorkItem&& task) {
    SM_ASSERT(gMainQueue != nullptr);
    gMainQueue->add(std::move(name), std::move(task));
}

void ThreadService::pollMainQueue() {
    SM_ASSERT(gMainQueue != nullptr);
    gMainQueue->tryGetMessage();
}

// scheduler

void ThreadService::setWorkerCount(size_t count) {
    if (count == 0) {
        count = gCpuGeometry.cores.size();
        LOG_INFO("worker count not specified, defaulting to {}", count);
    }

    auto maxCount = cfgMaxWorkerCount.getCurrentValue();
    if (maxCount > 0 && count > maxCount) {
        LOG_WARN("worker count {0} exceeds max worker count {1}, clamping to {1}", count, maxCount);
        count = maxCount;
    }

    LOG_INFO("starting {} workers", count);

    mt::WriteLock lock(getPoolLock());

    while (gWorkers.size() < count) {
        gWorkers.push_back(newWorkerThread());
    }

    while (gWorkers.size() > count) {
        auto *pWorker = gWorkers.back();
        std::erase(gThreadHandles, pWorker); // TODO: should we use a set instead?

        gWorkers.pop_back();
        pWorker->requestStop();
        delete pWorker;
    }
}

size_t ThreadService::getWorkerCount() {
    mt::ReadLock lock(getPoolLock());
    return gWorkers.size();
}

void ThreadService::enqueueWork(std::string name, threads::WorkItem&& func) {
    SM_ASSERT(gWorkQueue != nullptr);
    gWorkQueue->add(std::move(name), std::move(func));
}

threads::ThreadHandle *ThreadService::newThread(threads::ThreadType type, std::string name, threads::ThreadStart&& start) {
    auto *pHandle = newThreadInner(type, name, std::move(start));

    mt::WriteLock lock(getPoolLock());
    getPool().push_back(pHandle);
    return pHandle;
}

threads::ThreadHandle *ThreadService::newWorkerThread() {
    const auto kWorkerBody = [](std::stop_token token) {
        WorkMessage msg;
        auto interval = std::chrono::milliseconds(cfgWorkerDelay.getCurrentValue());

        while (!token.stop_requested()) {
            if (gWorkQueue->tryGetMessage(msg, interval)) {
                msg.item();
            }
        }
    };

    auto id = std::format("work.{}", gWorkerId++);
    auto *pHandle = ThreadService::newThreadInner(threads::eWorker, id, kWorkerBody);
    gThreadHandles.push_back(pHandle);
    return pHandle;
}

threads::ThreadHandle *ThreadService::newThreadInner(threads::ThreadType type, std::string name, threads::ThreadStart&& start) {
    auto *pHandle = new threads::ThreadHandle({
        .type = type,
        .mask = ScheduleMask({ }),
        .name = name,
        .start = std::move(start)
    });

    return pHandle;
}

void ThreadService::shutdown() {
    mt::WriteLock lock(getPoolLock());
    auto& handles = getPool();

    for (auto *pHandle : handles) {
        pHandle->join();
    }

    for (auto *pHandle : handles) {
        delete pHandle;
    }
    handles.clear();
}

mt::SharedMutex &ThreadService::getPoolLock() { return gThreadLock; }
std::vector<threads::ThreadHandle*> &ThreadService::getPool() { return gThreadHandles; }
