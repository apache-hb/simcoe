#include "engine/threads/thread.h"

#include "engine/core/error.h"
#include "engine/threads/service.h"
#include "engine/log/service.h"
#include "engine/debug/service.h"

#include "engine/core/unique.h"

#include <intsafe.h>
#include <string_view>

using namespace simcoe;
using namespace simcoe::threads;

using namespace std::chrono_literals;

template<typename T>
struct std::formatter<GROUP_AFFINITY, T> : std::formatter<std::string_view, T> {
    template<typename FormatContext>
    auto format(const GROUP_AFFINITY& affinity, FormatContext& ctx) {
        auto it = std::format("(group = {}, mask = {:#b})", affinity.Group, affinity.Mask);
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
        threads::setThreadName(std::string(pInfo->name));
        debug::setThreadName(pInfo->name);

        LOG_INFO("thread {:#06x} started", id);
        pInfo->start(pInfo->token);
        LOG_INFO("thread {:#06x} stopped", id);
        return 0;
    } catch (const core::Error& err) {
        LOG_ERROR("thread {:#06x} failed with engine error: {}", id, err.what());
        return 99;
    } catch (const std::exception& err) {
        LOG_ERROR("thread {:#06x} failed with exception: {}", id, err.what());
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

    if (hThread == nullptr) {
        debug::throwLastError("CreateThread");
    }

    const GROUP_AFFINITY affinity = mask;
    if (SetThreadGroupAffinity(hThread, &affinity, nullptr) == 0) {
        auto msg = std::format("SetThreadGroupAffinity failed. thread affinity mask: {}", affinity);
        debug::throwLastError(msg);
    }

    if (ResumeThread(hThread) == DWORD_MAX) {
        debug::throwLastError("ResumeThread");
    }

    LOG_INFO("created thread (name={}, id={:#06x}) with mask {}", name, id, affinity);
}

ThreadHandle::~ThreadHandle() {
    if (hThread == nullptr) return;

    // TODO: make sure the thread closes
    join();

    CloseHandle(hThread);
}

void ThreadHandle::join() {
    stopper.request_stop();
    if (WaitForSingleObject(hThread, INFINITE) != WAIT_OBJECT_0) {
        debug::throwLastError(std::format("WaitForSingleObject failed for thread (name={}, id={:#06x})", name, id));
    }
}
