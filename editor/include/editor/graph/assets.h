#pragma once

#include "engine/render/graph.h"

using namespace simcoe;
using namespace simcoe::render;

namespace editor::graph {
    using ITextureHandle = ISingleResourceHandle<rhi::TextureBuffer>;

    template<typename T>
    struct IUniformHandle : ISingleResourceHandle<rhi::UniformBuffer>, ISingleSRVHandle {
        IUniformHandle(Context *ctx, std::string name)
            : ISingleResourceHandle(ctx, name)
        { }

        void create() override {
            auto *pResource = ctx->createUniformBuffer(sizeof(T));
            setResource(pResource);
            setSrvIndex(ctx->mapUniform(pResource, sizeof(T)));
            setCurrentState(rhi::ResourceState::eShaderResource);
        }

        void destroy() override {
            auto *pSrvHeap = ctx->getSrvHeap();
            pSrvHeap->release(getSrvIndex());
            delete getResource();
        }

        void update(T *pData) {
            getBuffer()->write(pData, sizeof(T));
        }
    };

    struct SwapChainHandle final : IResourceHandle, IRTVHandle {
        SwapChainHandle(Context *ctx)
            : IResourceHandle(ctx, "swapchain.rtv", StateDep(eDepDisplaySize | eDepBackBufferCount))
        { }

        void create() override;
        void destroy() override;

        rhi::ResourceState getCurrentState() const override;
        void setCurrentState(rhi::ResourceState state) override;
        rhi::DeviceResource *getResource() const override;
        RenderTargetAlloc::Index getRtvIndex() const override;

    private:
        struct RenderTarget {
            rhi::RenderTarget *pRenderTarget;
            RenderTargetAlloc::Index rtvIndex;
            rhi::ResourceState state;
        };

        std::vector<RenderTarget> targets;
    };

    struct SceneTargetHandle final : ITextureHandle, ISingleSRVHandle, ISingleRTVHandle {
        SceneTargetHandle(Context *ctx)
            : ISingleResourceHandle(ctx, "texture.rtv", eDepRenderSize)
        { }

        static constexpr math::float4 kClearColour = { 0.0f, 0.2f, 0.4f, 1.0f };

        void create() override;
        void destroy() override;
    };

    struct TextureHandle final : ITextureHandle, ISingleSRVHandle {
        TextureHandle(Context *ctx, std::string name);

        void create() override;
        void destroy() override;

    private:
        std::string name;
    };
}
