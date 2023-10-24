#include "editor/game/level.h"
#include "editor/game/game.h"

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

IGameObject::IGameObject(GameLevel *pLevel, std::string name)
    : pLevel(pLevel)
    , name(name)
    , debugHandle(std::make_unique<debug::DebugHandle>(name, [this] { objectDebug(); }))
{
    game::Instance *pInstance = game::getInstance();
    pMesh = pInstance->getDefaultMesh();
    pTexture = pInstance->getDefaultTexture();
}

void IGameObject::setTexture(const fs::path& path) {
    game::getInstance()->loadTexture(path, [this](auto *pNewTexture) {
        pTexture = pNewTexture;
    });
}

void IGameObject::setMesh(const fs::path& path) {
    game::getInstance()->loadMesh(path, [this](auto *pNewMesh) {
        pMesh = pNewMesh;
    });
}

void IGameObject::retire() {
    pLevel->deleteObject(this);
}

void IGameObject::objectDebug() {
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

    debug();
}

// level

void GameLevel::deleteObject(IGameObject *pObject) {
    std::lock_guard guard(lock);
    retired.emplace(pObject);
}

void GameLevel::beginTick() {
    std::lock_guard guard(lock);

    for (IGameObject *pObject : pending)
        objects.emplace_back(pObject);

    pending.clear();
}

void GameLevel::endTick() {
    std::lock_guard guard(lock);

    for (IGameObject *pObject : retired) {
        std::erase(objects, pObject);
        //delete pObject; TODO: make sure all async tasks related to this object are done
    }

    retired.clear();
}

void GameLevel::debug() {
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
