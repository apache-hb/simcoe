#pragma once

#include "editor/graph/assets.h"

namespace editor::graph {
    using namespace simcoe::math;

    struct UNIFORM_BUFFER MipMapInfo {
        uint32_t sourceLevel;
        uint32_t mipLevel;
        float2 texelSize;
    };

    struct MipMapInfoHandle : IUniformHandle<MipMapInfo> {
        MipMapInfoHandle(Graph *ctx);
    };

    struct RwTextureHandle : ISingleResourceHandle<rhi::RwTextureBuffer>, ISingleUAVHandle {
        RwTextureHandle(Graph *ctx, uint2 size, size_t mipLevel);

        void create() override;
        void destroy() override;

    private:
        uint2 size;
        size_t mipLevel;
    };

    struct MipMapPass : ICommandPass {
        MipMapPass(Graph *ctx, ResourceWrapper<TextureHandle> *pSourceTexture);

        void create() override;
        void destroy() override;

        void execute() override;

    private:
        PassAttachment<ISRVHandle> *pSourceTexture;

        ResourceWrapper<MipMapInfoHandle> *pMipMapInfo;
        PassAttachment<MipMapInfoHandle> *pMipMapInfoAttachment;

        ResourceWrapper<RwTextureHandle> *pTargetTexture;
        PassAttachment<IUAVHandle> *pTargetTextureAttachment;

        rhi::PipelineState *pPipelineState;
    };
}
