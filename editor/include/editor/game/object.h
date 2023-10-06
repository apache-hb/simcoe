#pragma once

#include "engine/render/assets.h"

namespace editor {
    namespace math = simcoe::math;
    namespace render = simcoe::render;

    struct UNIFORM_ALIGN StaticMeshInfo {
        math::float3 position;
        math::float3 rotation;
        math::float3 scale;
    };

    struct StaticMesh {
        StaticMeshInfo info;

        render::VertexBufferHandle vertexBuffer;
        render::IndexBufferHandle indexBuffer;
    };
}
