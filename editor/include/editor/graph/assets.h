#pragma once

#include "engine/render/graph.h"

using namespace simcoe;
using namespace simcoe::render;

namespace editor::graph {
    using ITextureHandle = ISingleResourceHandle<rhi::TextureBuffer>;

    struct Vertex {
        math::float3 position;
        math::float2 uv;
    };

    template<typename T>
    struct IUniformHandle : ISingleResourceHandle<rhi::UniformBuffer>, ISingleSRVHandle {
        IUniformHandle(Graph *ctx, std::string name, StateDep stateDeps = eDepDevice)
            : ISingleResourceHandle(ctx, name, stateDeps)
        { }

        void create() override {
            auto *pResource = ctx->createUniformBuffer(sizeof(T));
            setResource(pResource);
            setSrvIndex(ctx->mapUniform(pResource, sizeof(T)));
            setCurrentState(rhi::ResourceState::eShaderResource);
        }

        void destroy() override {
            ISingleSRVHandle::destroy(ctx);
            ISingleResourceHandle::destroy();
        }

        void update(T *pData) {
            getBuffer()->write(pData, sizeof(T));
        }
    };

    struct SwapChainHandle final : IResourceHandle, IRTVHandle {
        SwapChainHandle(Graph *ctx)
            : IResourceHandle(ctx, "swapchain.rtv", StateDep(eDepDisplaySize | eDepBackBufferCount))
        { }

        void create() override;
        void destroy() override;

        rhi::DeviceResource *getResource() const override;
        RenderTargetAlloc::Index getRtvIndex() const override;

    private:
        struct RenderTarget {
            rhi::RenderTarget *pRenderTarget;
            RenderTargetAlloc::Index rtvIndex;
        };

        std::vector<RenderTarget> targets;
    };

    struct SceneTargetHandle final : ITextureHandle, ISingleSRVHandle, ISingleRTVHandle {
        SceneTargetHandle(Graph *ctx)
            : ISingleResourceHandle(ctx, "texture.rtv", eDepRenderSize)
        { }

        static constexpr math::float4 kClearColour = { 0.0f, 0.2f, 0.4f, 1.0f };

        void create() override;
        void destroy() override;
    };

    struct TextureHandle final : ITextureHandle, ISingleSRVHandle {
        TextureHandle(Graph *ctx, std::string name);

        void create() override;
        void destroy() override;

    private:
        std::string name;
    };
}
