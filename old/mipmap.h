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
        MipMapInfoHandle(Graph *pGraph);
    };

    struct RwTextureHandle : ISingleResourceHandle<rhi::RwTextureBuffer>, ISingleUAVHandle {
        RwTextureHandle(Graph *pGraph, uint2 size, size_t mipLevel);

        void create() override;
        void destroy() override;

    private:
        uint2 size;
        size_t mipLevel;
    };

    struct MipMapPass : ICommandPass {
        MipMapPass(Graph *pGraph, ResourceWrapper<TextureHandle> *pSourceTexture, size_t mipLevels);

        void create() override;
        void destroy() override;

        void execute() override;

    private:
        PassAttachment<ISRVHandle> *pSourceTexture;

        ResourceWrapper<MipMapInfoHandle> *pMipMapInfo;
        PassAttachment<MipMapInfoHandle> *pMipMapInfoAttachment;

        struct MipMapTarget {
            ResourceWrapper<RwTextureHandle> *pTargetTexture;
            PassAttachment<IUAVHandle> *pTargetTextureAttachment;
        };

        std::unique_ptr<MipMapTarget[]> pMipMapTargets;
        size_t mipLevels;

        rhi::PipelineState *pPipelineState;
    };
}
