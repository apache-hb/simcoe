#pragma once

#include "editor/graph/assets.h"

namespace editor {
    struct GameLevel {
        math::float2 playerOffset;
        float playerAngle = 0.f;
        float playerScale = 1.f;
    };

    struct GameState {
        void loadLevel(GameLevel level);
    };
}
