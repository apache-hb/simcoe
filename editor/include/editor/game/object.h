#pragma once

#include "engine/render/assets.h"

namespace editor {
    using namespace simcoe;
    namespace fs = assets::fs;

    struct ObjVertex {
        constexpr bool operator==(const ObjVertex& other) const = default;

        math::float3 position;
        math::float2 uv;
    };

    struct ObjMesh final : render::IMeshBufferHandle {
        ObjMesh(render::Graph *ctx, const fs::path& path);

        void create() override;
        void destroy() override;

        size_t getIndexCount() const override { return indexData.size(); }

        rhi::IndexBuffer *getIndexBuffer() const override { return pIndexBuffer; }
        rhi::VertexBuffer *getVertexBuffer() const override { return pVertexBuffer; }

    private:
        fs::path path;
        void loadAsset();

        std::vector<ObjVertex> vertexData;
        std::vector<uint16_t> indexData;

        rhi::VertexBuffer *pVertexBuffer;
        rhi::IndexBuffer *pIndexBuffer;
    };

    struct GltfVertex {
        math::float3 position;
        math::float2 uv;
    };

    struct GltfMesh final : render::IMeshBufferHandle {
        GltfMesh(render::Graph *ctx, std::string path)
            : IMeshBufferHandle(ctx, path)
            , path(path)
        { }

        void create() override;
        void destroy() override;

        size_t getIndexCount() const override;

        rhi::IndexBuffer *getIndexBuffer() const override;
        rhi::VertexBuffer *getVertexBuffer() const override;

    private:
        std::string path;
    };
}
