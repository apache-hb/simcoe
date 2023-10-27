#pragma once

#include "engine/math/math.h"
#include "engine/math/sparse.h"

#include <unordered_set>

namespace simcoe::physics {
    using namespace simcoe::math;

    struct RigidBody {
        float3 origin;
        float3 velocity;
    };

    struct World {
        World();

        void tick(float delta);

        void addBody(RigidBody *pBody);

    private:
        void updateBody(RigidBody *pBody);

        std::vector<RigidBody*> bodies;
    };
}
