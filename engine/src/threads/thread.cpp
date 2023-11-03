#include "engine/threads/thread.h"

#include "engine/threads/service.h"
#include "engine/log/service.h"
#include "engine/service/debug.h"

#include "engine/core/unique.h"

#include <intsafe.h>
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

// thread handle

namespace {
    DWORD runThread(core::UniquePtr<ThreadStartInfo> pInfo, ThreadId id) try {
        LOG_INFO("thread {:#06x} started", id);
        pInfo->start(pInfo->token);
        LOG_INFO("thread {:#06x} stopped", id);
        return 0;
    } catch (std::exception& err) {
        LOG_ERROR("thread {:#06x} failed with exception: {}", id, err.what());
        return 99;
    } catch (...) {
        LOG_ERROR("thread {:#06x} failed with unknown exception", id);
        return 99;
    }
}

DWORD WINAPI ThreadHandle::threadThunk(LPVOID lpParameter) {
    ThreadId id = ThreadService::getCurrentThreadId();
    return runThread(static_cast<ThreadStartInfo*>(lpParameter), id);
}

ThreadHandle::ThreadHandle(ThreadInfo&& info)
    : type(info.type)
    , mask(info.mask)
    , name(info.name)
{
    auto *pStart = new ThreadStartInfo {
        .start = std::move(info.start),
        .token = stopper.get_token(),
        .name = name
    };

    hThread = CreateThread(
        /*lpThreadAttributes=*/ nullptr,
        /*dwStackSize=*/ 0,
        /*lpStartAddress=*/ ThreadHandle::threadThunk,
        /*lpParameter=*/ pStart, // deleted by threadThunk
        /*dwCreationFlags=*/ CREATE_SUSPENDED,
        /*lpThreadId=*/ &id
    );

    if (hThread == nullptr) { throwLastError("CreateThread"); }

    ThreadService::setThreadName(std::string(name), id);

    const GROUP_AFFINITY affinity = mask;
    if (SetThreadGroupAffinity(hThread, &affinity, nullptr) == 0) {
        auto msg = std::format("SetThreadGroupAffinity failed, internal state corruption!!! thread affinity mask: {}", affinity);
        throwLastError(msg);
    }

    if (ResumeThread(hThread) == DWORD_MAX) {
        throwLastError("ResumeThread");
    }

    LOG_INFO("created thread (name={}, id={:#06x}) with mask {}", ThreadService::getThreadName(id), id, affinity);
}

ThreadHandle::~ThreadHandle() {
    if (hThread == nullptr) return;
    stopper.request_stop();

    // TODO: make sure the thread closes
    if (WaitForSingleObject(hThread, INFINITE) != WAIT_OBJECT_0) {
        throwLastError(std::format("WaitForSingleObject failed for thread (name={}, id={:#06x})", name, id));
    }

    CloseHandle(hThread);
}
