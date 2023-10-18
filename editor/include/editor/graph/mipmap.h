#pragma once

#include "editor/graph/assets.h"

namespace editor::graph {
    using namespace simcoe::math;

    using uint2 = Vec2<uint32_t>;

    struct UNIFORM_BUFFER MipMapInfo {
        uint32_t sourceLevel;
        uint32_t mipLevel;
        float2 texelSize;
    };

    struct MipMapInfoHandle : IUniformHandle<MipMapInfo> {
        MipMapInfoHandle(Graph *ctx);
    };

    struct MipMapPass : ICommandPass {
        MipMapPass(Graph *ctx, ResourceWrapper<ITextureHandle> *pSourceTexture);

        void create() override;
        void destroy() override;

        void execute() override;

    private:
        PassAttachment<ITextureHandle> *pSourceTexture;

        ResourceWrapper<MipMapInfoHandle> *pMipMapInfo;
        PassAttachment<MipMapInfoHandle> *pMipMapInfoAttachment;

        rhi::PipelineState *pPipelineState;
    };
}
