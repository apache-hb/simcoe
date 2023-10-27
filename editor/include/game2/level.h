#pragma once

#include "game2/physics.h"

namespace game {
    struct Level {
        PhysicsWorld *getPhysicsWorld() { return &physics; }

    private:
        PhysicsWorld physics;
    };
}
