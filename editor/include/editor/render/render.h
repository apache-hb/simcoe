#pragma once

#include "engine/assets/assets.h"
#include "engine/render/render.h"

namespace editor {
    using namespace simcoe;

    // forward declarations

    struct RenderContext;

    // external api types

    struct RenderCreateInfo {
        HWND hWindow;
        assets::Assets& depot;

        UINT displayWidth;
        UINT displayHeight;

        UINT renderWidth;
        UINT renderHeight;
    };

    // shader data types

    struct Vertex {
        math::float3 position;
        math::float2 uv;
    };

    // helper types

    template<typename T>
    struct DescriptorAlloc {
        enum struct Index : size_t { };

        DescriptorAlloc(render::DescriptorHeap *pHeap)
            : pHeap(pHeap)
            , offset(0)
        { }

        ~DescriptorAlloc() {
            delete pHeap;
        }

        Index alloc() {
            return Index(offset++);
        }

        render::HostHeapOffset hostOffset(Index index) const {
            return pHeap->hostOffset(size_t(index));
        }

        render::DeviceHeapOffset deviceOffset(Index index) const {
            return pHeap->deviceOffset(size_t(index));
        }

        render::DescriptorHeap *pHeap;
        size_t offset;
    };

    struct RenderTargetHeap;
    struct ShaderDataHeap;

    using RenderTargetAlloc = DescriptorAlloc<RenderTargetHeap>;
    using DataAlloc = DescriptorAlloc<ShaderDataHeap>;

    struct FrameData {
        render::RenderTarget *pRenderTarget;
        RenderTargetAlloc::Index renderTargetHeapIndex;

        render::CommandMemory *pMemory;
        size_t fenceValue = 1;
    };

    struct RenderTarget {
        render::TextureBuffer *pTarget;
        RenderTargetAlloc::Index index;
    };

    struct RenderContext {
        static constexpr UINT kBackBufferCount = 2;
        static constexpr math::float4 kClearColour = { 0.0f, 0.2f, 0.4f, 1.0f };
        static constexpr math::float4 kBlackClearColour = { 0.0f, 0.0f, 0.0f, 1.0f };

        static RenderContext *create(const RenderCreateInfo& createInfo);

        ~RenderContext();

        void beginRender();
        void endRender();

        void executeScene(
            DataAlloc::Index quadUniformIndex,
            const render::Display& display, 
            const RenderTarget& target
        );
        void executePost(
            const render::Display& display, 
            render::PipelineState *pPostPipeline, 
            DataAlloc::Index sceneTarget
        );

        void executePresent();

        // getters
        const RenderCreateInfo& getCreateInfo() const { return createInfo; }

        // create resources
        render::TextureBuffer *createTextureRenderTarget(const render::TextureInfo& createInfo, const math::float4& clearColour) {
            return pDevice->createTextureRenderTarget(createInfo, clearColour);
        }
        
        render::UniformBuffer *createUniformBuffer(size_t size) {
            return pDevice->createUniformBuffer(size);
        }

        render::PipelineState *createPipelineState(const render::PipelineCreateInfo& createInfo) {
            return pDevice->createPipelineState(createInfo);
        }

        // heap allocators
        RenderTargetAlloc::Index mapRenderTarget(render::DeviceResource *pResource) {
            auto index = pRenderTargetAlloc->alloc();
            pDevice->mapRenderTarget(pRenderTargetAlloc->hostOffset(index), pResource);
            return index;
        }

        DataAlloc::Index mapTexture(render::TextureBuffer *pResource) {
            auto index = pDataAlloc->alloc();
            pDevice->mapTexture(pDataAlloc->hostOffset(index), pResource);
            return index;
        }

        DataAlloc::Index mapUniform(render::UniformBuffer *pBuffer, size_t size) {
            auto index = pDataAlloc->alloc();
            pDevice->mapUniform(pDataAlloc->hostOffset(index), pBuffer, size);
            return index;
        }

        // commands
        void transition(render::DeviceResource *pResource, render::ResourceState from, render::ResourceState to) {
            pDirectCommands->transition(pResource, from, to);
        }

    private:
        RenderContext(const RenderCreateInfo& createInfo);

        // state selection

        render::Adapter* selectAdapter() {
            return adapters[0];
        }

        // create data that depends on the context
        void createContextData();
        void destroyContextData();

        // create data that depends on the device
        void createDeviceData(render::Adapter* pAdapter);
        void destroyDeviceData();

        // create data that depends on the present queue
        void createDisplayData();
        void destroyDisplayData();

        void createPostData();
        void destroyPostData();

        void createResources();
        void destroyResources();

        // rendering

        void waitForCopy();

        RenderCreateInfo createInfo;

        // device data

        render::Context *pContext;
        std::vector<render::Adapter*> adapters;
        render::Device *pDevice;

        render::DeviceQueue *pDirectQueue;

        render::Commands *pDirectCommands;

        // device copy data

        render::DeviceQueue *pCopyQueue;
        render::CommandMemory *pCopyAllocator;
        render::Commands *pCopyCommands;
        size_t copyFenceValue = 1;

        size_t frameIndex = 0;
        render::Fence *pFence;

        FrameData frameData[kBackBufferCount];

        // swapchain resolution dependant data

        render::DisplayQueue *pDisplayQueue;

        RenderTargetAlloc *pRenderTargetAlloc;
        DataAlloc *pDataAlloc;

        // present resolution dependant data

        render::VertexBuffer *pScreenQuadVerts;
        render::IndexBuffer *pScreenQuadIndices;

        // scene resources

        render::PipelineState *pScenePipeline;
        render::VertexBuffer *pQuadVertexBuffer;
        render::IndexBuffer *pQuadIndexBuffer;

        render::TextureBuffer *pTextureBuffer;
        DataAlloc::Index quadTextureIndex;
    };
}
