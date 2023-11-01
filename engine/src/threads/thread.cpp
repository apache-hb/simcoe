#include "engine/threads/thread.h"

#include "engine/threads/service.h"
#include "engine/service/logging.h"
#include "engine/service/debug.h"

#include <string_view>

using namespace simcoe;
using namespace simcoe::threads;

template<typename T>
struct std::formatter<GROUP_AFFINITY, T> : std::formatter<std::string_view, T> {
    template<typename FormatContext>
    auto format(const GROUP_AFFINITY& affinity, FormatContext& ctx) {
        auto it = std::format("(group = {}, mask = {:#b})", affinity.Group, affinity.Mask);
        return std::formatter<std::string_view, T>::format(it, ctx);
    }
};

namespace {
    LogicalThread pickThreadForCore(const PhysicalCore& core) {
        const auto& geometry = ThreadService::getGeometry();
        const auto& thread = geometry.threads[core.threadIds[0]]; // TODO: delegate to a scheduler
        return thread;
    }

    LogicalThread pickThreadForCluster(const CoreCluster& cluster) {
        const auto& geometry = ThreadService::getGeometry();
        const auto& core = geometry.cores[cluster.coreIds[0]];
        const auto& thread = geometry.threads[core.threadIds[0]]; // TODO: delegate to a scheduler
        return thread;
    }

    LogicalThread pickAnyThread() {
        const auto& geometry = ThreadService::getGeometry();
        const auto& thread = geometry.threads[0]; // TODO: delegate to a scheduler
        return thread;
    }

    struct ThreadStartInfo {
        ThreadStart start;
        std::stop_token token;
    };
}

DWORD WINAPI Thread::threadThunk(LPVOID lpParameter) try {
    std::unique_ptr<ThreadStartInfo> pInfo {static_cast<ThreadStartInfo*>(lpParameter)};

    LOG_INFO("thread {:#x} started", ThreadService::getCurrentThreadId());
    pInfo->start(pInfo->token);
    LOG_INFO("thread {:#x} stopped", ThreadService::getCurrentThreadId());
    return 0;
} catch (std::exception& err) {
    LOG_ERROR("thread {:#x} failed with exception: {}", ThreadService::getCurrentThreadId(), err.what());
    return 99;
} catch (...) {
    LOG_ERROR("thread {:#x} failed with unknown exception", ThreadService::getCurrentThreadId());
    return 99;
}

Thread::Thread(Thread&& other) noexcept {
    std::swap(id, other.id);
    std::swap(hThread, other.hThread);
    std::swap(threadIdx, other.threadIdx);
    std::swap(stopper, other.stopper);
}

Thread& Thread::operator=(Thread&& other) noexcept {
    std::swap(id, other.id);
    std::swap(hThread, other.hThread);
    std::swap(threadIdx, other.threadIdx);
    std::swap(stopper, other.stopper);
    return *this;
}

Thread::Thread(const LogicalThread& thread, ThreadStart&& start)
    : threadIdx(thread)
{
    auto *pStart = new ThreadStartInfo {
        .start = std::move(start),
        .token = stopper.get_token()
    };

    hThread = CreateThread(
        /*lpThreadAttributes=*/ nullptr,
        /*dwStackSize=*/ 0,
        /*lpStartAddress=*/ Thread::threadThunk,
        /*lpParameter=*/ pStart, // deleted by threadThunk
        /*dwCreationFlags=*/ CREATE_SUSPENDED,
        /*lpThreadId=*/ &id
    );

    if (hThread == nullptr) {
        throwLastError("CreateThread");
    }

    const GROUP_AFFINITY affinity = threadIdx.mask.affinity;

    if (!SetThreadGroupAffinity(hThread, &affinity, nullptr)) {
        auto msg = std::format("SetThreadGroupAffinity failed, internal state corruption!!! thread affinity mask: {}", affinity);
        throwLastError(msg);
    }

    if (ResumeThread(hThread) == -1) {
        throwLastError("ResumeThread");
    }

    LOG_INFO("created os thread {:#x} on logical thread {}", GetThreadId(hThread), threadIdx.mask.affinity);
}

Thread::Thread(const PhysicalCore& core, ThreadStart&& start)
    : Thread(pickThreadForCore(core), std::move(start))
{ }

Thread::Thread(const CoreCluster& cluster, ThreadStart&& start)
    : Thread(pickThreadForCluster(cluster), std::move(start))
{ }

Thread::Thread(ThreadStart&& start)
    : Thread(pickAnyThread(), std::move(start))
{ }

Thread::~Thread() {
    if (hThread == nullptr) return;

    LOG_INFO("requesting stop (id: {:#x} handle: {})", id, (void*)hThread);
    stopper.request_stop();

    CloseHandle(hThread);
}
