#pragma once

#include "game2/physics.h"

namespace game {
    struct Level {
        void addCollider(ICollider *pCollider) {
            physics.addCollider(pCollider);
        }

        void removeCollider(ICollider *pCollider) {
            physics.removeCollider(pCollider);
        }
    private:
        PhysicsWorld physics;
    };
}
