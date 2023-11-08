#include "editor/ui/service.h"

#include "engine/core/mt.h"

#include "imgui/imgui.h"

#include <unordered_set>

using namespace simcoe;

using namespace editor;
using namespace editor::ui;

// global debug handles

namespace {
    mt::shared_mutex gLock;
    std::unordered_set<ui::DebugHandle*> gHandles;
}

ui::GlobalHandle ui::addGlobalHandle(const std::string &name, std::function<void ()> draw) {
    DebugHandle *pHandle = new DebugHandle(name, draw);
    GlobalHandle userHandle(pHandle, [](auto *pHandle) {
        ui::removeGlobalHandle(pHandle);
        delete pHandle;
    });

    mt::write_lock lock(gLock);
    gHandles.insert(pHandle);

    return userHandle;
}

void ui::removeGlobalHandle(DebugHandle *pHandle) {
    mt::write_lock lock(gLock);
    gHandles.erase(pHandle);
}

void ui::enumGlobalHandles(std::function<void(DebugHandle*)> callback) {
    mt::read_lock lock(gLock);
    for (auto *pHandle : gHandles) {
        callback(pHandle);
    }
}

// service debuggers

void ServiceUi::drawMenuItem() {
    auto name = getServiceName();
    ImGui::MenuItem(name.data(), nullptr, &bOpen);
}

void ServiceUi::drawWindow() {
    if (!bOpen) return;

    auto name = getServiceName();
    if (ImGui::Begin(name.data(), &bOpen)) {
        auto err = getServiceError();
        if (!err.empty()) {
            ImGui::Text("Failed to initialize: %s", err.data());
        } else {
            draw();
        }
    }

    ImGui::End();
}

std::string_view ServiceUi::getServiceName() const {
    return serviceName;
}

std::string_view ServiceUi::getServiceError() const {
    return serviceError;
}

void ServiceUi::setServiceError(std::string_view reason) {
    serviceError = reason;
}
