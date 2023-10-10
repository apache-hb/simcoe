#pragma once

#include "editor/game/object.h"

#include <DirectXMath.h>

namespace editor {
    struct GameObject {
        std::string name;

        math::float3 position = { 0.0f, 0.0f, 0.0f };
        math::float3 rotation = { 0.0f, 0.0f, 0.0f }; // rotate around z-axis
        math::float3 scale = { 1.f, 1.f, 1.f };
    };

    struct GameLevel {
        std::vector<GameObject> objects;

        math::float3 cameraPosition = { 5.0f, 5.0f, 5.0f };
        float fov = 90.f;
    };
}
