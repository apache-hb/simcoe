#include "game/world.h"

using namespace game;

static size_t gUniqueId = 0;
size_t game::getUniqueId() {
    return gUniqueId++;
}

TypeTag game::makeNameInfo(World *pWorld, const std::string &name) {
    static std::unordered_map<std::string, TypeInfo> infos;
    
    if (auto it = infos.find(name); it != infos.end()) {
        return it->second;
    }

    TypeInfo info = { pWorld, getUniqueId() };
    auto [iter, inserted] = infos.emplace(name, info);
    return iter->second;
}

void IEntity::addComponent(IComponent *pComponent) {
    components.emplace(pComponent->getTypeInfo(), pComponent);
}
