#include "game/level.h"
#include "game/game.h"

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

IEntity::IEntity(GameLevel *pLevel, std::string name, size_t id)
    : pLevel(pLevel)
    , id(id)
    , name(name)
    , debugHandle(std::make_unique<debug::DebugHandle>(name, [this] { objectDebug(); }))
{
    game::Instance *pInstance = game::getInstance();
    pMesh = pInstance->getDefaultMesh();
    pTexture = pInstance->getDefaultTexture();
}

void IEntity::setTexture(const fs::path& path) {
    if (currentTexture == path)
        return;

    currentTexture = path;
    game::getInstance()->loadTexture(path, [this](auto *pNewTexture) {
        pTexture = pNewTexture;
    });
}

void IEntity::setMesh(const fs::path& path) {
    if (currentMesh == path)
        return;

    currentMesh = path;
    game::getInstance()->loadMesh(path, [this](auto *pNewMesh) {
        pMesh = pNewMesh;
    });
}

void IEntity::retire() {
    pLevel->deleteObject(this);
}

void IEntity::objectDebug() {
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

void GameLevel::deleteObject(IEntity *pObject) {
    std::lock_guard guard(lock);
    retired.emplace(pObject);
}

void GameLevel::beginTick() {
    std::lock_guard guard(lock);

    for (IEntity *pObject : pending)
        objects.emplace_back(pObject);

    pending.clear();
}

void GameLevel::endTick() {
    std::lock_guard guard(lock);

    for (IEntity *pObject : retired) {
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
