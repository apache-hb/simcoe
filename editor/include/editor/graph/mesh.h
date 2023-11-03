#pragma once

#include "engine/render/assets.h"

namespace editor::graph {
    using namespace simcoe;
    using namespace simcoe::math;

    struct ObjVertex {
        constexpr bool operator==(const ObjVertex& other) const = default;

        float3 position;
        float2 uv;
    };

    struct ObjMesh final : render::ISingleMeshBufferHandle {
        ObjMesh(render::Graph *pGraph, const fs::path& path);

        void create() override;

        size_t getIndexCount() const override { return indexData.size(); }

    private:
        fs::path path;
        void loadAsset();

        std::vector<ObjVertex> vertexData;
        std::vector<uint16_t> indexData;
    };
}
