#include "editor/debug/debug.h"

#include "imgui/imgui.h"
#include <unordered_set>

using namespace simcoe;

using namespace editor;
using namespace editor::debug;

namespace {
    std::mutex gLock;
    std::unordered_set<debug::DebugHandle*> gHandles;
}

debug::GlobalHandle debug::addGlobalHandle(const std::string &name, std::function<void ()> draw) {
    DebugHandle *pHandle = new DebugHandle(name, draw);
    GlobalHandle userHandle(pHandle, [](auto *pHandle) {
        debug::removeGlobalHandle(pHandle);
        delete pHandle;
    });

    std::lock_guard lock(gLock);
    gHandles.insert(pHandle);

    return userHandle;
}

void debug::removeGlobalHandle(DebugHandle *pHandle) {
    std::lock_guard lock(gLock);
    gHandles.erase(pHandle);
}

void debug::enumGlobalHandles(std::function<void(DebugHandle*)> callback) {
    std::lock_guard lock(gLock);
    for (auto *pHandle : gHandles) {
        callback(pHandle);
    }
}
