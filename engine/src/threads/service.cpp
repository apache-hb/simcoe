#include "engine/threads/service.h"

#include "engine/service/logging.h"

using namespace simcoe;

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

    // GetLogicalProcessorInformationEx provides information about inter-core communication
    struct ProcessorInfoIterator {
        ProcessorInfoIterator() { setupInfo(); }

        bool next() {
            if (remaining == 0) return false;

            remaining -= pBuffer->Size;
            if (remaining > 0) {
                pBuffer = advance(pBuffer, pBuffer->Size);
            }

            return true;
        }

        SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *getCurrent() const {
            return pBuffer;
        }

    private:
        void setupInfo() {
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

        std::unique_ptr<std::byte[]> memory;
        SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *pBuffer;
        DWORD bufferSize = 0;

        DWORD remaining = 0;
    };

    // GetSystemCpuSetInformation provides information about cpu speeds
    struct CpuSetIterator {
        CpuSetIterator() { setupInfo(); }

        bool next() {
            if (remaining == 0) return false;

            remaining -= pBuffer->Size;
            if (remaining > 0) {
                pBuffer = advance(pBuffer, pBuffer->Size);
            }

            return true;
        }

        SYSTEM_CPU_SET_INFORMATION *getCurrent() const {
            return pBuffer;
        }
    private:
        void setupInfo() {
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
        size_t packageCount = 0;
        size_t coreCount = 0;
        size_t cacheCount = 0;

        void addProcessorCore(const PROCESSOR_RELATIONSHIP& info) {
            bool hasSmt = info.Flags & LTP_PC_SMT;
            LOG_INFO("RelationProcessorCore: (smt={}, class={}, groups={})", hasSmt, info.EfficiencyClass, info.GroupCount);
            for (DWORD i = 0; i < info.GroupCount; ++i) {
                GROUP_AFFINITY group = info.GroupMask[i];
                LOG_INFO("  Group[{}]: group={} mask=0x{:x}", i, group.Group, group.Mask);
            }

            coreCount += 1;
        }

        void addProcessorPackage(const PROCESSOR_RELATIONSHIP& info) {
            LOG_INFO("RelationProcessorPackage: (flags={}, class={}, groups={})", info.Flags, info.EfficiencyClass, info.GroupCount);
            for (DWORD i = 0; i < info.GroupCount; ++i) {
                GROUP_AFFINITY group = info.GroupMask[i];
                LOG_INFO("  Group[{}]: group={} mask=0x{:x}", i, group.Group, group.Mask);
            }

            packageCount += 1;
        }

        void addCache(const CACHE_RELATIONSHIP& info) {
            LOG_INFO("RelationCache: (level={}, associativity={}, lineSize={}, size={}, type={})", info.Level, info.Associativity, info.LineSize, info.CacheSize, info.Type);
            for (DWORD i = 0; i < info.GroupCount; ++i) {
                GROUP_AFFINITY group = info.GroupMasks[i];
                LOG_INFO("  Group[{}]: group={} mask=0x{:x}", i, group.Group, group.Mask);
            }

            cacheCount += 1;
        }

        void addGroup(const GROUP_RELATIONSHIP& info) {
            LOG_INFO("RelationGroup: (maximumGroupCount={}, activeGroupCount={})", info.MaximumGroupCount, info.ActiveGroupCount);
            for (DWORD i = 0; i < info.ActiveGroupCount; ++i) {
                PROCESSOR_GROUP_INFO group = info.GroupInfo[i];
                LOG_INFO("  ProcessorGroup[{}]: max={} active={} mask=0x{:x}", i, group.MaximumProcessorCount, group.ActiveProcessorCount, group.ActiveProcessorMask);
            }
        }

        void addNumaNode(const NUMA_NODE_RELATIONSHIP& info) {
            LOG_INFO("RelationNumaNode: (nodeNumber={})", info.NodeNumber);
            for (DWORD i = 0; i < info.GroupCount; ++i) {
                GROUP_AFFINITY group = info.GroupMasks[i];
                LOG_INFO("  Group[{}]: group={} mask=0x{:x}", i, group.Group, group.Mask);
            }
        }
    };

    struct CpuSetLayout {
        size_t cpuSetCount = 0;

        void addCpuSet(const SYSTEM_CPU_SET_INFORMATION& info) {
            auto cpuSet = info.CpuSet;
            LOG_INFO("CpuSet: (id={}, group={}, procIndex={}, coreIndex={}, class={}, flags=0x{:x}, schedule={}, tag={})",
                cpuSet.Id,
                cpuSet.Group,
                cpuSet.LogicalProcessorIndex,
                cpuSet.CoreIndex,
                cpuSet.EfficiencyClass,
                cpuSet.AllFlags,
                cpuSet.SchedulingClass,
                cpuSet.AllocationTag
            );

            cpuSetCount += 1;
        }
    };
}

bool ThreadService::createService() {
    ProcessorInfoIterator processorInfo;
    ProcessorInfoLayout processorInfoLayout;
    do {
        auto *pCurrent = processorInfo.getCurrent();

        switch (pCurrent->Relationship) {
        case RelationProcessorPackage:
            processorInfoLayout.addProcessorPackage(pCurrent->Processor);
            break;

        case RelationProcessorCore:
            processorInfoLayout.addProcessorCore(pCurrent->Processor);
            break;

        case RelationCache:
            processorInfoLayout.addCache(pCurrent->Cache);
            break;

        case RelationGroup:
            processorInfoLayout.addGroup(pCurrent->Group);
            break;

        case RelationNumaNode:
            processorInfoLayout.addNumaNode(pCurrent->NumaNode);
            break;

        case RelationProcessorModule:
            break; // ignore these

        default:
            LOG_INFO("Unknown processor relationship: {}", pCurrent->Relationship);
            break;
        }
    } while (processorInfo.next());

    LOG_INFO("CPU layout: (packages={} cores={} caches={})", processorInfoLayout.packageCount, processorInfoLayout.coreCount, processorInfoLayout.cacheCount);

    CpuSetIterator cpuSetInfo;
    CpuSetLayout cpuSetLayout;

    do {
        auto *pCurrent = cpuSetInfo.getCurrent();
        switch (pCurrent->Type) {
        case CpuSetInformation:
            cpuSetLayout.addCpuSet(*pCurrent);
            break;

        default:
            LOG_INFO("Unknown cpu set type: {}", pCurrent->Type);
            break;
        }
    } while (cpuSetInfo.next());

    LOG_INFO("CPU sets: (count={})", cpuSetLayout.cpuSetCount);

    return true;
}

void ThreadService::destroyService() {

}
