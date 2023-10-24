#pragma once

#include "editor/debug/debug.h"

#include "engine/render/graph.h"

using namespace simcoe;
using namespace simcoe::render;
using namespace simcoe::math;

namespace editor::graph {
    namespace fs = assets::fs;
    using ITextureHandle = ISingleResourceHandle<rhi::TextureBuffer>;

    struct Vertex {
        float3 position;
        float2 uv;
    };

    template<typename T>
    struct IUniformHandle : ISingleResourceHandle<rhi::UniformBuffer>, ISingleSRVHandle {
        static_assert(alignof(T) <= UNIFORM_ALIGN, "Uniform buffer must be 256-byte aligned");

        IUniformHandle(Graph *ctx, std::string name, StateDep stateDeps = eDepDevice)
            : ISingleResourceHandle(ctx, name, stateDeps)
        { }

        void create() override {
            auto *pResource = ctx->createUniformBuffer(sizeof(T));
            setResource(pResource);
            setSrvIndex(ctx->mapUniform(pResource, sizeof(T)));
            setCurrentState(rhi::ResourceState::eUniform);
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
        SwapChainHandle(Graph *ctx);

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
        SceneTargetHandle(Graph *ctx);

        void create() override;
        void destroy() override;
    };

    struct DepthTargetHandle final : ISingleResourceHandle<rhi::DepthBuffer>, ISingleDSVHandle {
        DepthTargetHandle(Graph *ctx)
            : ISingleResourceHandle(ctx, "depth.dsv", eDepRenderSize)
        { }

        void create() override;
        void destroy() override;
    };

    struct TextureHandle final : ITextureHandle, ISingleSRVHandle {
        TextureHandle(Graph *ctx, std::string name);

        void create() override;
        void destroy() override;

        uint2 getSize() const { return uint2::from(image.width, image.height); }

    private:
        // image path
        std::string name;

        // image data
        assets::Image image;
    };

    struct TextHandle final : ITextureHandle, ISingleSRVHandle {
        TextHandle(Graph *ctx, std::string_view name, std::u32string text);

        void create() override;
        void destroy() override;

    private:
        assets::Font font;
        assets::Image bitmap;
        std::u32string text;
    };
}
