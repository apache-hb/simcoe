#pragma once

#include "engine/math/math.h"

#include "game2/components/collider.h"
#include <unordered_set>

namespace game {
    struct RayHit {
        ICollider *pCollider = nullptr;
        float distance = 0.f;
    };

    struct PhysicsWorld {
        bool rayCast(const RayCast& cast, RayHit *pHit) const;

        void addCollider(ICollider *pCollider);
        void removeCollider(ICollider *pCollider);
    private:
        std::unordered_set<ICollider*> colliders;
    };
}
