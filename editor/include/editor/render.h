#pragma once

#include "engine/assets/assets.h"
#include "engine/render/render.h"

namespace editor {
    using namespace simcoe;

    struct UNIFORM_ALIGN UniformData {
        math::float2 offset;

        float angle;
        float aspect;
    };

    struct RenderCreateInfo {
        HWND hWindow;
        assets::Assets& depot;

        UINT displayWidth;
        UINT displayHeight;

        UINT renderWidth;
        UINT renderHeight;
    };

    struct Vertex {
        math::float3 position;
        math::float2 uv;
    };

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

    struct RenderContext {
        static constexpr UINT kBackBufferCount = 2;
        static constexpr math::float4 kClearColour = { 0.0f, 0.2f, 0.4f, 1.0f };
        static constexpr math::float4 kBlackClearColour = { 0.0f, 0.0f, 0.0f, 1.0f };

        static RenderContext *create(const RenderCreateInfo& createInfo);

        ~RenderContext();
        void render(float time);

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

        void updateUniform(float time);

        void executeScene();
        void executePost();

        void beginFrame();
        void endFrame();

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

        render::Display postDisplay;

        render::DisplayQueue *pDisplayQueue;

        RenderTargetAlloc *pRenderTargetAlloc;
        DataAlloc *pDataAlloc;

        // render resolution dependant data

        render::Display sceneDisplay;

        render::TextureBuffer *pSceneTarget;
        RenderTargetAlloc::Index sceneRenderTargetIndex;
        DataAlloc::Index screenTextureIndex;

        // present resolution dependant data

        render::PipelineState *pPostPipeline;

        render::VertexBuffer *pScreenQuadVerts;
        render::IndexBuffer *pScreenQuadIndices;

        // scene resources

        render::PipelineState *pScenePipeline;
        render::VertexBuffer *pQuadVertexBuffer;
        render::IndexBuffer *pQuadIndexBuffer;

        render::TextureBuffer *pTextureBuffer;
        DataAlloc::Index quadTextureIndex;

        render::UniformBuffer *pQuadUniformBuffer;
        DataAlloc::Index quadUniformIndex;
    };
}
