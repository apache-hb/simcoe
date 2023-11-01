#include "engine/threads/thread.h"

#include "engine/threads/service.h"
#include "engine/service/logging.h"
#include "engine/service/debug.h"

#include <string_view>

using namespace simcoe;
using namespace simcoe::threads;

namespace {
    constexpr uint8_t first_bit(uint64_t bits) {
        for (uint8_t i = 0; i < 64; ++i) {
            if (bits & (1ull << i)) {
                return i;
            }
        }

        return 0;
    }
}

template<typename T>
struct std::formatter<GROUP_AFFINITY, T> : std::formatter<std::string_view, T> {
    template<typename FormatContext>
    auto format(const GROUP_AFFINITY& affinity, FormatContext& ctx) {
        auto it = std::format("(group = {}, mask = {})", affinity.Group, first_bit(affinity.Mask));
        return std::formatter<std::string_view, T>::format(it, ctx);
    }
};

struct ThreadStartInfo {
    ThreadStart start;
    std::stop_token token;
    std::string_view name;
};

DWORD WINAPI Thread::threadThunk(LPVOID lpParameter) try {
    std::unique_ptr<ThreadStartInfo> pInfo {static_cast<ThreadStartInfo*>(lpParameter)};
    DebugService::setThreadName(pInfo->name);

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
    std::swap(subcore, other.subcore);
    std::swap(stopper, other.stopper);
}

Thread& Thread::operator=(Thread&& other) noexcept {
    std::swap(id, other.id);
    std::swap(hThread, other.hThread);
    std::swap(subcore, other.subcore);
    std::swap(stopper, other.stopper);
    return *this;
}

Thread::Thread(const Subcore& subcore, std::string_view name, ThreadStart&& start)
    : subcore(subcore)
{
    auto *pStart = new ThreadStartInfo {
        .start = std::move(start),
        .token = stopper.get_token(),
        .name = name
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

    const GROUP_AFFINITY affinity = subcore.mask;

    if (!SetThreadGroupAffinity(hThread, &affinity, nullptr)) {
        auto msg = std::format("SetThreadGroupAffinity failed, internal state corruption!!! thread affinity mask: {}", affinity);
        throwLastError(msg);
    }

    if (ResumeThread(hThread) == -1) {
        throwLastError("ResumeThread");
    }

    LOG_INFO("created os thread {:#x} on logical thread {}", GetThreadId(hThread), affinity);
}

Thread::~Thread() {
    if (hThread == nullptr) return;

    LOG_INFO("requesting stop (id: {:#x} handle: {})", id, (void*)hThread);
    stopper.request_stop();

    CloseHandle(hThread);
}
