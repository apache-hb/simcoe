#pragma once

#include "engine/render/assets.h"

namespace editor {
    using namespace simcoe;

    struct ObjVertex {
        math::float3 position;
        math::float2 uv;
    };

    struct ObjMesh : render::IMeshBufferHandle {
        ObjMesh(render::Context *ctx, std::string path, std::string basedir)
            : IMeshBufferHandle(ctx, path)
            , path(path)
            , basedir(basedir)
        { }

        void create() override;
        void destroy() override;

        size_t getIndexCount() const override { return indexCount; }
        std::vector<rhi::VertexAttribute> getVertexAttributes() const override;

        rhi::IndexBuffer *getIndexBuffer() const override { return pIndexBuffer; }
        rhi::VertexBuffer *getVertexBuffer() const override { return pVertexBuffer; }

    private:
        std::string path;
        std::string basedir;

        size_t indexCount;
        rhi::VertexBuffer *pVertexBuffer;
        rhi::IndexBuffer *pIndexBuffer;
    };
}
