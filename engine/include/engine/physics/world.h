#pragma once

#include "engine/math/math.h"

namespace simcoe::physics {
    using namespace simcoe::math;

    struct Rect {
        float2 position;
        float2 size;
    };

    struct World {
        World(float gridSize);

    private:

    };
}
