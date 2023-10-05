#pragma once

#include "engine/render/graph.h"

using namespace simcoe;
using namespace simcoe::render;

namespace editor::graph {
    using ITextureHandle = IShaderResourceHandle<rhi::TextureBuffer>;
    using IUniformHandle = IShaderResourceHandle<rhi::UniformBuffer>;

    struct SwapChainHandle final : IResourceHandle {
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

    struct SceneTargetHandle final : ITextureHandle {
        SceneTargetHandle(Context *ctx)
            : ITextureHandle(ctx, "texture.rtv", eDepRenderSize)
        { }

        static constexpr math::float4 kClearColour = { 0.0f, 0.2f, 0.4f, 1.0f };

        void create() override;
        void destroy() override;

        RenderTargetAlloc::Index getRtvIndex() const override {
            return rtvIndex;
        }

    private:
        RenderTargetAlloc::Index rtvIndex;
    };

    struct TextureHandle final : ITextureHandle {
        TextureHandle(Context *ctx, std::string name);

        void create() override;

    private:
        std::string name;
    };
}
