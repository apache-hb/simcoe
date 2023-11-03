#include "editor/debug/debug.h"

#include "engine/core/mt.h"

#include "imgui/imgui.h"
#include <unordered_set>

using namespace simcoe;

using namespace editor;
using namespace editor::debug;

// global debug handles

namespace {
    mt::shared_mutex gLock;
    std::unordered_set<debug::DebugHandle*> gHandles;
}

debug::GlobalHandle debug::addGlobalHandle(const std::string &name, std::function<void ()> draw) {
    DebugHandle *pHandle = new DebugHandle(name, draw);
    GlobalHandle userHandle(pHandle, [](auto *pHandle) {
        debug::removeGlobalHandle(pHandle);
        delete pHandle;
    });

    mt::write_lock lock(gLock);
    gHandles.insert(pHandle);

    return userHandle;
}

void debug::removeGlobalHandle(DebugHandle *pHandle) {
    mt::write_lock lock(gLock);
    gHandles.erase(pHandle);
}

void debug::enumGlobalHandles(std::function<void(DebugHandle*)> callback) {
    mt::read_lock lock(gLock);
    for (auto *pHandle : gHandles) {
        callback(pHandle);
    }
}

// service debuggers

void ServiceDebug::drawMenuItem() {
    ImGui::MenuItem(name, nullptr, &bOpen);
}

void ServiceDebug::drawWindow() {
    if (!bOpen) return;

    if (ImGui::Begin(name, &bOpen)) {
        auto err = getFailureReason();
        if (!err.empty()) {
            ImGui::Text("Failed to initialize: %s", err.data());
        } else {
            draw();
        }
    }

    ImGui::End();
}

void ServiceDebug::setFailureReason(std::string_view reason) {
    failureReason = reason;
}
