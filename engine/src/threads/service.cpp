#include "engine/threads/service.h"

#include "engine/service/logging.h"
#include <unordered_set>

using namespace simcoe;
using namespace simcoe::threads;

template<typename T>
struct std::formatter<LOGICAL_PROCESSOR_RELATIONSHIP, T> : std::formatter<std::string_view, T> {
    template<typename FormatContext>
    auto format(LOGICAL_PROCESSOR_RELATIONSHIP rel, FormatContext& ctx) {
        using f = std::formatter<std::string_view, T>;
        switch (rel) {
        case RelationProcessorCore: return f::format("RelationNumaNode", ctx);
        case RelationNumaNode: return f::format("RelationNumaNode", ctx);
        case RelationCache: return f::format("RelationCache", ctx);
        case RelationProcessorPackage: return f::format("RelationProcessorPackage", ctx);
        case RelationGroup: return f::format("RelationGroup", ctx);
        case RelationProcessorDie: return f::format("RelationProcessorDie", ctx);
        case RelationNumaNodeEx: return f::format("RelationNumaNodeEx", ctx);
        case RelationProcessorModule: return f::format("RelationProcessorModule", ctx);
        default: return f::format(std::format("LOGICAL_PROCESSOR_RELATIONSHIP({})", int(rel)), ctx);
        }
    }
};

template<typename T>
struct std::formatter<PROCESSOR_CACHE_TYPE, T> : std::formatter<std::string_view, T> {
    template<typename FormatContext>
    auto format(PROCESSOR_CACHE_TYPE type, FormatContext& ctx) {
        using f = std::formatter<std::string_view, T>;
        switch (type) {
        case CacheUnified: return f::format("CacheUnified", ctx);
        case CacheInstruction: return f::format("CacheInstruction", ctx);
        case CacheData: return f::format("CacheData", ctx);
        case CacheTrace: return f::format("CacheTrace", ctx);
        case CACHE_FULLY_ASSOCIATIVE: return f::format("CACHE_FULLY_ASSOCIATIVE", ctx);
        default: return f::format(std::format("PROCESSOR_CACHE_TYPE({})", int(type)), ctx);
        }
    }
};

template<typename T>
struct std::formatter<CPU_SET_INFORMATION_TYPE, T> : std::formatter<std::string_view, T> {
    template<typename FormatContext>
    auto format(CPU_SET_INFORMATION_TYPE type, FormatContext& ctx) {
        using f = std::formatter<std::string_view, T>;
        switch (type) {
        case CpuSetInformation: return f::format("CpuSetInformation", ctx);
        default: return f::format(std::format("CPU_SET_INFORMATION_TYPE({})", int(type)), ctx);
        }
    }
};

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
            ASSERTF(remaining > 0, "iterator overrun");

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
                throw std::runtime_error("GetLogicalProcessorInformationEx did not fail");
            }

            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
                throwLastError("GetLogicalProcessorInformationEx did not fail with ERROR_INSUFFICIENT_BUFFER");
            }

            memory = std::make_unique<std::byte[]>(bufferSize);
            pBuffer = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(memory.get());
            if (!GetLogicalProcessorInformationEx(relation, pBuffer, &bufferSize)) {
                throwLastError("GetLogicalProcessorInformationEx failed");
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
            ASSERTF(remaining > 0, "iterator overrun");

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
                throw std::runtime_error("GetSystemCpuSetInformation did not fail");
            }

            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
                throwLastError("GetSystemCpuSetInformation did not fail with ERROR_INSUFFICIENT_BUFFER");
            }

            memory = std::make_unique<std::byte[]>(bufferSize);
            pBuffer = reinterpret_cast<SYSTEM_CPU_SET_INFORMATION *>(memory.get());
            if (!GetSystemCpuSetInformation(pBuffer, bufferSize, &bufferSize, GetCurrentProcess(), 0)) {
                throwLastError("GetSystemCpuSetInformation failed");
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
        std::vector<LogicalThread> threads;
        std::vector<PhysicalCore> cores;
        std::vector<CoreCluster> clusters;
        std::vector<Package> packages;

        template<typename T>
        static void getItemByMask(std::vector<uint16_t>& ids, const std::vector<T>& items, GROUP_AFFINITY affinity) {
            std::unordered_set<uint16_t> uniqueIds;
            for (size_t i = 0; i < items.size(); ++i) {
                const auto& item = items[i];
                if (item.mask.affinity.Group == affinity.Group && item.mask.affinity.Mask & affinity.Mask) {
                    uniqueIds.insert(i);
                }
            }

            for (uint16_t id : uniqueIds) {
                ids.push_back(id);
            }
        }

        void getPhysicalCoresByMask(std::vector<uint16_t>& ids, GROUP_AFFINITY affinity) const {
            getItemByMask(ids, cores, affinity);
        }

        void getLogicalThreadsByMask(std::vector<uint16_t>& ids, GROUP_AFFINITY affinity) const {
            getItemByMask(ids, threads, affinity);
        }

        void getCoreClustersByMask(std::vector<uint16_t>& ids, GROUP_AFFINITY affinity) const {
            getItemByMask(ids, clusters, affinity);
        }
    };

    struct ProcessorInfoLayout {
        ProcessorInfoLayout(GeometryBuilder *pBuilder)
            : pBuilder(pBuilder)
        { }

        GeometryBuilder *pBuilder;
        size_t cacheCount = 0;

        void addProcessorCore(const PROCESSOR_RELATIONSHIP *pInfo) {
            std::vector<uint16_t> threadIds;

            for (DWORD i = 0; i < pInfo->GroupCount; ++i) {
                GROUP_AFFINITY group = pInfo->GroupMask[i];

                for (size_t i = 0; i < 64; ++i) {
                    KAFFINITY mask = 1ull << i;
                    if (group.Mask & mask) {
                        pBuilder->threads.push_back({
                            .mask = { .affinity = group }
                        });

                        uint16_t id = pBuilder->threads.size() - 1;
                        threadIds.push_back(id);
                    }
                }
            }

            pBuilder->cores.push_back({
                .efficiency = pInfo->EfficiencyClass,
                .mask = { .affinity = pInfo->GroupMask[0] },
                .threadIds = threadIds
            });
        }

        void addProcessorPackage(const PROCESSOR_RELATIONSHIP *pInfo) {
            std::vector<uint16_t> coreIds;
            std::vector<uint16_t> threadIds;
            std::vector<uint16_t> clusterIds;

            for (DWORD i = 0; i < pInfo->GroupCount; ++i) {
                GROUP_AFFINITY group = pInfo->GroupMask[i];
                pBuilder->getPhysicalCoresByMask(coreIds, group);
                pBuilder->getLogicalThreadsByMask(threadIds, group);
                pBuilder->getCoreClustersByMask(clusterIds, group);
            }

            pBuilder->packages.push_back({
                .mask = { .affinity = pInfo->GroupMask[0] },
                .cores = coreIds,
                .threads = threadIds,
                .clusters = clusterIds
            });
        }

        void addCache(const CACHE_RELATIONSHIP& info) {
            if (info.Level != 3) return;

            // assume everything that shares l3 cache is in the same cluster
            // TODO: on intel E-cores and P-cores share the same l3
            //       but we dont want to put them in the same cluster.
            //       for now that isnt a big problem though.
            std::vector<uint16_t> coreIds;

            for (DWORD i = 0; i < info.GroupCount; ++i) {
                GROUP_AFFINITY group = info.GroupMasks[i];
                pBuilder->getPhysicalCoresByMask(coreIds, group);
            }

            pBuilder->clusters.push_back({
                .mask = { .affinity = info.GroupMasks[0] },
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
                .Mask = 1ul << KAFFINITY(cpuSet.LogicalProcessorIndex),
                .Group = cpuSet.Group
            };

            std::vector<uint16_t> coreIds;
            pBuilder->getPhysicalCoresByMask(coreIds, groupAffinity);
            for (uint16_t coreId : coreIds) {
                pBuilder->cores[coreId].schedule = cpuSet.SchedulingClass;
            }
        }
    };
}

bool ThreadService::createService() {
    GeometryBuilder builder;
    ProcessorInfoLayout processorInfoLayout{&builder};
    CpuSetLayout cpuSetLayout{&builder};

    ProcessorInfo coreInfo{RelationProcessorCore};
    ProcessorInfo cacheInfo{RelationCache};
    ProcessorInfo packageInfo{RelationProcessorPackage};

    for (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *pRelation : coreInfo) {
        switch (pRelation->Relationship) {
        case RelationProcessorCore:
            processorInfoLayout.addProcessorCore(&pRelation->Processor);
            break;

        default:
            break;
        }
    }

    for (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *pRelation : cacheInfo) {
        switch (pRelation->Relationship) {
        case RelationCache:
            processorInfoLayout.addCache(pRelation->Cache);
            break;

        default:
            break;
        }
    }

    for (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *pRelation : packageInfo) {
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

    LOG_INFO("CPU layout: (packages={} cores={} threads={} caches={})", builder.packages.size(), builder.cores.size(), builder.threads.size(), processorInfoLayout.cacheCount);

    geometry = {
        .threads = builder.threads,
        .cores = builder.cores,
        .clusters = builder.clusters,
        .packages = builder.packages
    };

    return true;
}

void ThreadService::destroyService() {

}

std::string_view ThreadService::getFailureReason() {
    return get()->failureReason;
}

ThreadId ThreadService::getCurrentThreadId() {
    return std::this_thread::get_id();
}

Geometry ThreadService::getGeometry() {
    return get()->geometry;
}

void ThreadService::migrateCurrentThread(const LogicalThread& thread) {
    HANDLE hThread = GetCurrentThread();
    GROUP_AFFINITY groupAffinity = thread.mask.affinity;

    if (!SetThreadGroupAffinity(hThread, &groupAffinity, nullptr)) {
        throwLastError("SetThreadGroupAffinity failed");
    }
}

// c1: 14
// c2: 14
// c3: 9
// c4: 12
// c5: 8
// c6: 11
// c7: 10
// c8: 13

// c9: 7
// c10: 6
// c11: 2
// c12: 4
// c13: 0
// c14: 5
// c15: 3
// c16: 1
