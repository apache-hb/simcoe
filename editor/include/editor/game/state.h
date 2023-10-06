#pragma once

#include "editor/game/object.h"

namespace editor {
    struct GameLevel {
        math::float3 playerPosition = { 0.0f, 0.0f, 0.0f };
        math::float3 playerRotation = { 0.0f, 0.0f, 0.0f };
        math::float3 playerScale = { 1.f, 1.f, 1.f };

        math::float3 cameraPosition = { 5.0f, 5.0f, 5.0f };
        float fov = 90.f;
    };
}
