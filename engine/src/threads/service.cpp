#include "engine/threads/service.h"

#include "engine/service/logging.h"

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
        ProcessorInfo() {
            if (GetLogicalProcessorInformationEx(RelationAll, nullptr, &bufferSize)) {
                throw std::runtime_error("GetLogicalProcessorInformationEx did not fail");
            }

            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
                throwLastError("GetLogicalProcessorInformationEx did not fail with ERROR_INSUFFICIENT_BUFFER");
            }

            memory = std::make_unique<std::byte[]>(bufferSize);
            pBuffer = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(memory.get());
            if (!GetLogicalProcessorInformationEx(RelationAll, pBuffer, &bufferSize)) {
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

    struct ThreadIndex {
        WORD Group;
        WORD Mask;
    };

    struct ProcessorInfoLayout {
        size_t threadCount = 0;
        size_t cacheCount = 0;

        std::vector<const PROCESSOR_RELATIONSHIP*> cores;
        std::vector<const PROCESSOR_RELATIONSHIP*> packages;

        std::vector<LogicalThread> threads;

        void addProcessorCore(const PROCESSOR_RELATIONSHIP *pInfo) {
            bool hasSmt = pInfo->Flags & LTP_PC_SMT;
            LOG_INFO("RelationProcessorCore: (smt={}, class={}, groups={})", hasSmt, pInfo->EfficiencyClass, pInfo->GroupCount);
            for (DWORD i = 0; i < pInfo->GroupCount; ++i) {
                GROUP_AFFINITY group = pInfo->GroupMask[i];
                LOG_INFO("  Group[{}]: group={} mask=0b{:064b}", i, group.Group, group.Mask);

                threadCount += std::popcount(group.Mask);

                LogicalThread thread = {
                    .type = hasSmt ? eThreadEfficiency : eThreadPerformance,
                };

                threads.push_back(thread);
            }

            cores.push_back(pInfo);
        }

        void addProcessorPackage(const PROCESSOR_RELATIONSHIP *pInfo) {
            LOG_INFO("RelationProcessorPackage: (flags={}, class={}, groups={})", pInfo->Flags, pInfo->EfficiencyClass, pInfo->GroupCount);
            for (DWORD i = 0; i < pInfo->GroupCount; ++i) {
                GROUP_AFFINITY group = pInfo->GroupMask[i];
                LOG_INFO("  Group[{}]: group={} mask=0b{:064b}", i, group.Group, group.Mask);
            }

            packages.push_back(pInfo);
        }

        void addCache(const CACHE_RELATIONSHIP& info) {
            LOG_INFO("RelationCache: (level={}, associativity={}, lineSize={}, size={}, type={})", info.Level, info.Associativity, info.LineSize, info.CacheSize, info.Type);
            for (DWORD i = 0; i < info.GroupCount; ++i) {
                GROUP_AFFINITY group = info.GroupMasks[i];
                LOG_INFO("  Group[{}]: group={} mask=0b{:064b}", i, group.Group, group.Mask);
            }

            cacheCount += 1;
        }

        void addGroup(const GROUP_RELATIONSHIP& info) {
            LOG_INFO("RelationGroup: (maximumGroupCount={}, activeGroupCount={})", info.MaximumGroupCount, info.ActiveGroupCount);
            for (DWORD i = 0; i < info.ActiveGroupCount; ++i) {
                PROCESSOR_GROUP_INFO group = info.GroupInfo[i];
                LOG_INFO("  ProcessorGroup[{}]: max={} active={} mask=0b{:064b}", i, group.MaximumProcessorCount, group.ActiveProcessorCount, group.ActiveProcessorMask);
            }
        }

        void addNumaNode(const NUMA_NODE_RELATIONSHIP& info) {
            LOG_INFO("RelationNumaNode: (nodeNumber={})", info.NodeNumber);
            for (DWORD i = 0; i < info.GroupCount; ++i) {
                GROUP_AFFINITY group = info.GroupMasks[i];
                LOG_INFO("  Group[{}]: group={} mask=0b{:064b}", i, group.Group, group.Mask);
            }
        }
    };

    struct CpuSetLayout {
        size_t cpuSetCount = 0;

        void addCpuSet(const SYSTEM_CPU_SET_INFORMATION *pInfo) {
            auto cpuSet = pInfo->CpuSet;
            LOG_INFO("CpuSet: (id={}, group={}, procIndex={}, coreIndex={}, class={}, flags=0x{:x}, schedule={}, tag={}, cache={})",
                cpuSet.Id,
                cpuSet.Group,
                cpuSet.LogicalProcessorIndex,
                cpuSet.CoreIndex,
                cpuSet.EfficiencyClass,
                cpuSet.AllFlags,
                cpuSet.SchedulingClass,
                cpuSet.AllocationTag,
                cpuSet.LastLevelCacheIndex
            );

            cpuSetCount += 1;
        }
    };
}

bool ThreadService::createService() {
    ProcessorInfo processorInfo;
    ProcessorInfoLayout processorInfoLayout;
    for (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *pRelation : processorInfo) {

        switch (pRelation->Relationship) {
        case RelationProcessorPackage:
            processorInfoLayout.addProcessorPackage(&pRelation->Processor);
            break;

        case RelationProcessorCore:
            processorInfoLayout.addProcessorCore(&pRelation->Processor);
            break;

        case RelationCache:
            processorInfoLayout.addCache(pRelation->Cache);
            break;

        case RelationGroup:
            processorInfoLayout.addGroup(pRelation->Group);
            break;

        case RelationNumaNode:
            processorInfoLayout.addNumaNode(pRelation->NumaNode);
            break;

        case RelationProcessorModule:
            break; // ignore these

        default:
            LOG_INFO("Unknown processor relationship: {}", pRelation->Relationship);
            break;
        }
    }

    LOG_INFO("CPU layout: (packages={} cores={} threads={} caches={})", processorInfoLayout.packages.size(), processorInfoLayout.cores.size(), processorInfoLayout.threadCount, processorInfoLayout.cacheCount);

    CpuSetInfo cpuSetInfo;
    CpuSetLayout cpuSetLayout;

    for (SYSTEM_CPU_SET_INFORMATION *pCpuSet : cpuSetInfo) {
        switch (pCpuSet->Type) {
        case CpuSetInformation:
            cpuSetLayout.addCpuSet(pCpuSet);
            break;

        default:
            LOG_INFO("Unknown cpu set type: {}", pCpuSet->Type);
            break;
        }
    }

    LOG_INFO("CPU sets: (count={})", cpuSetLayout.cpuSetCount);

    return true;
}

void ThreadService::destroyService() {

}

ThreadId ThreadService::getCurrentThreadId() {
    return std::this_thread::get_id();
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
