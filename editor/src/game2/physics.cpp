#include "game2/physics.h"

using namespace game;

bool PhysicsWorld::rayCast(const RayCast& cast, RayHit *pHit) const {
    ASSERT(pHit != nullptr);

    float distance = cast.distance;
    ICollider *pClosest = nullptr;
    for (ICollider *pCollider : colliders) {
        float other = pCollider->rayIntersects(cast);
        if (other < distance) {
            distance = other;
            pClosest = pCollider;
        }
    }

    if (pClosest) {
        pHit->distance = distance;
        pHit->pCollider = pClosest;
        return true;
    }

    return false;
}

void PhysicsWorld::addCollider(ICollider *pCollider) {
    colliders.emplace(pCollider);
}

void PhysicsWorld::removeCollider(ICollider *pCollider) {
    colliders.erase(pCollider);
}
