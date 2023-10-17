#include "editor/game/level.h"

using namespace editor;

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
