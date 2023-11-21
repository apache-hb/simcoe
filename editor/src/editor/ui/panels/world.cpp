#include "editor/ui/panels/world.h"

#include "game/service.h"

using namespace editor;
using namespace editor::ui;

WorldUi::WorldUi()
    : ServiceUi("ECS World")
{ }

void WorldUi::draw() {
    mt::ReadLock lock(game::GameService::getWorldMutex());

    auto& world = game::GameService::getWorld();

    const auto& entities = world.entities;
    //const auto& objects = world.objects;

    ImGui::Text("World (%zu entities)", entities.getUsed());

    if (ImGui::CollapsingHeader("Entities")) {
        for (auto *pEntity : world.all()) {
            const auto& name = pEntity->getName();
            auto info = pEntity->getTypeInfo();
            if (ImGui::TreeNode((void*)pEntity, "Entity: %s (typeid: %zu)", name.c_str(), info.getId())) {

                for (const auto& [id, pComponent] : pEntity->getComponents()) {
                    auto compInfo = pComponent->getTypeInfo();
                    const auto& compName = pComponent->getName();

                    if (ImGui::TreeNode((void*)compInfo.getId(), "Component: %s (typeid: %zu)", compName.c_str(), compInfo.getId())) {
                        pComponent->onDebugDraw();
                        ImGui::TreePop();
                    }
                }

                ImGui::TreePop();
            }

        }
    }
}
