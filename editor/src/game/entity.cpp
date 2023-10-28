#include "game/entity.h"
#include "game/game.h"

#include "imgui/imgui.h"

using namespace editor;
using namespace game;

// entity

IEntity::IEntity(Level *pLevel, std::string name, size_t id)
    : pLevel(pLevel)
    , id(id)
    , name(name)
    , debugHandle(std::make_unique<debug::DebugHandle>(name, [this] { objectDebug(); }))
{
    game::Instance *pInstance = game::getInstance();
    pMesh = pInstance->getDefaultMesh();
    pTexture = pInstance->getDefaultTexture();
}

IEntity::IEntity(const EntityInfo& info)
    : IEntity(info.pLevel, info.name, info.type)
{ }

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
