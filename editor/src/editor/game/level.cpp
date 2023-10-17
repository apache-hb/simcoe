#include "editor/game/level.h"

using namespace editor;

void GameLevel::deleteObject(IGameObject *pObject) {
    std::lock_guard guard(lock);
    retired.emplace(pObject);
}

void GameLevel::beginTick() {

}

void GameLevel::endTick() {
    std::lock_guard guard(lock);

    for (auto *pObject : retired) {
        std::erase(objects, pObject);
        delete pObject;
    }

    retired.clear();
}
