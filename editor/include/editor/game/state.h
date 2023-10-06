#pragma once

#include "editor/game/object.h"

namespace editor {
    struct GameLevel {
        math::float2 playerOffset;
        float playerAngle = 0.f;
        float playerScale = 1.f;

        std::vector<StaticMesh> meshes;
    };
}
