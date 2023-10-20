#include "editor/game/level.h"

#include "imgui/imgui.h"

using namespace editor;
using namespace editor::game;

// cameras

void Perspective::debug() {
    ImGui::SliderFloat("fov", &fov, 0.1f, 3.14f);
    ImGui::SliderFloat("near", &nearLimit, 0.1f, 100.f);
    ImGui::SliderFloat("far", &farLimit, 0.1f, 1000.f);
}

void Orthographic::debug() {
    ImGui::SliderFloat("width", &width, 0.1f, 100.f);
    ImGui::SliderFloat("height", &height, 0.1f, 100.f);
    ImGui::SliderFloat("near", &nearLimit, 0.1f, 100.f);
    ImGui::SliderFloat("far", &farLimit, 0.1f, 1000.f);
}

// object

void IGameObject::debug() {
    ImGui::InputFloat3("Position", position.data());
    ImGui::InputFloat3("Rotation", rotation.data());

    ImGui::Checkbox("Lock Scale", &bLockScale);
    ImGui::SameLine();
    if (bLockScale) {
        ImGui::InputFloat("Scale", &scale.x);
        scale.y = scale.x;
        scale.z = scale.x;
    } else {
        ImGui::InputFloat3("Scale", scale.data());
    }
}

// level

void GameLevel::deleteObject(IGameObject *pObject) {
    std::lock_guard guard(lock);
    retired.emplace(pObject);
}

void GameLevel::beginTick() {
    std::lock_guard guard(lock);

    for (auto *pObject : pending)
        objects.emplace_back(pObject);

    pending.clear();
}

void GameLevel::endTick() {
    std::lock_guard guard(lock);

    for (auto *pObject : retired) {
        std::erase(objects, pObject);
        delete pObject;
    }

    retired.clear();
}
