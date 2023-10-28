#include "game/level.h"
#include "game/game.h"

#include "imgui/imgui.h"

using namespace editor;
using namespace game;

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

// level

void Level::deleteObject(IEntity *pObject) {
    std::lock_guard guard(lock);
    retired.emplace(pObject);
}

void Level::beginTick() {
    std::lock_guard guard(lock);

    for (IEntity *pObject : pending)
        objects.emplace_back(pObject);

    pending.clear();
}

void Level::endTick() {
    std::lock_guard guard(lock);

    for (IEntity *pObject : retired) {
        std::erase(objects, pObject);
        //delete pObject; TODO: make sure all async tasks related to this object are done
    }

    retired.clear();
}

void Level::debug() {
    if (ImGui::CollapsingHeader("Objects")) {
        useEachObject([](auto pObject) {
            debug::DebugHandle *pDebug = pObject->getDebugHandle();
            ImGui::SeparatorText(pDebug->getName());

            ImGui::PushID(pObject);
            pDebug->draw();
            ImGui::PopID();
        });
    }
}
