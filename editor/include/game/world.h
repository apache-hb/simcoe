#pragma once

#include "vendor/flecs/flecs.h"

namespace game {
    /**
     * all public methods are thread safe
     *
     * all internal methods are not thread safe
     */
    struct World {
        World();

        flecs::world ecs;
    };
}
