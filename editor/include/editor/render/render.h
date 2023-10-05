#pragma once

#include "engine/assets/assets.h"
#include "engine/render/render.h"
#include "engine/memory/bitmap.h"

namespace editor {
    using namespace simcoe;

    // forward declarations

    struct RenderContext;

    // external api types

    struct RenderCreateInfo {
        HWND hWindow;
        assets::Assets& depot;

        size_t adapterIndex = 0;
        UINT backBufferCount = 2;

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
        using Index = engine::BitMap::Index;

        DescriptorAlloc(render::DescriptorHeap *pHeap, size_t size)
            : pHeap(pHeap)
            , mem(size)
        { }

        ~DescriptorAlloc() {
            delete pHeap;
        }

        void reset() {
            mem.reset();
        }

        Index alloc() {
            Index idx = mem.alloc();
            if (idx == Index::eInvalid) {
                throw std::runtime_error("out of descriptor heap space");
            }

            return idx;
        }

        void release(Index index) {
            mem.release(index);
        }

        render::HostHeapOffset hostOffset(Index index) const {
            return pHeap->hostOffset(size_t(index));
        }

        render::DeviceHeapOffset deviceOffset(Index index) const {
            return pHeap->deviceOffset(size_t(index));
        }

        render::DescriptorHeap *pHeap;
        engine::BitMap mem;
    };

    struct RenderTargetHeap;
    struct ShaderDataHeap;

    using RenderTargetAlloc = DescriptorAlloc<RenderTargetHeap>;
    using ShaderResourceAlloc = DescriptorAlloc<ShaderDataHeap>;

    struct FrameData {
        render::CommandMemory *pMemory;
    };

    struct RenderContext {
        static constexpr math::float4 kClearColour = { 0.0f, 0.2f, 0.4f, 1.0f };
        static constexpr math::float4 kBlackClearColour = { 0.0f, 0.0f, 0.0f, 1.0f };

        static RenderContext *create(const RenderCreateInfo& createInfo);

        ~RenderContext();

        void flush();

        void beginDirect();
        void endDirect();

        void beginRender();
        void endRender();

        void beginCopy();
        void endCopy();

        void waitForCopyQueue();
        void waitForDirectQueue();

        // actions

        void changeDisplaySize(UINT width, UINT height);
        void changeRenderSize(UINT width, UINT height);
        void changeBackBufferCount(UINT count);
        void changeAdapter(size_t index);

        // getters
        const RenderCreateInfo& getCreateInfo() const { return createInfo; }
        size_t getFrameIndex() const { return frameIndex; }

        std::vector<render::Adapter*> &getAdapters() { return adapters; }

        render::Device *getDevice() const { return pDevice; }
        render::Commands *getDirectCommands() const { return pDirectCommands; }

        ShaderResourceAlloc *getSrvHeap() { return pDataAlloc; }
        RenderTargetAlloc *getRtvHeap() { return pRenderTargetAlloc; }

        render::RenderTarget *getRenderTarget(size_t index) { return pDisplayQueue->getRenderTarget(index); }

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

        render::UploadBuffer *createUploadBuffer(const void *pData, size_t size) {
            return pDevice->createUploadBuffer(pData, size);
        }

        render::IndexBuffer *createIndexBuffer(size_t length, render::TypeFormat format) {
            return pDevice->createIndexBuffer(length, format);
        }

        render::VertexBuffer *createVertexBuffer(size_t length, size_t stride) {
            return pDevice->createVertexBuffer(length, stride);
        }

        render::UploadBuffer *createTextureUploadBuffer(const render::TextureInfo& info) {
            return pDevice->createTextureUploadBuffer(info);
        }

        render::TextureBuffer *createTexture(const render::TextureInfo& info) {
            return pDevice->createTexture(info);
        }

        // heap allocators
        RenderTargetAlloc::Index mapRenderTarget(render::DeviceResource *pResource) {
            auto index = pRenderTargetAlloc->alloc();
            pDevice->mapRenderTarget(pRenderTargetAlloc->hostOffset(index), pResource);
            return index;
        }

        ShaderResourceAlloc::Index mapTexture(render::TextureBuffer *pResource) {
            auto index = pDataAlloc->alloc();
            pDevice->mapTexture(pDataAlloc->hostOffset(index), pResource);
            return index;
        }

        ShaderResourceAlloc::Index mapUniform(render::UniformBuffer *pBuffer, size_t size) {
            auto index = pDataAlloc->alloc();
            pDevice->mapUniform(pDataAlloc->hostOffset(index), pBuffer, size);
            return index;
        }

        ShaderResourceAlloc::Index allocSrvIndex() {
            return pDataAlloc->alloc();
        }

        // commands
        void transition(render::DeviceResource *pResource, render::ResourceState from, render::ResourceState to) {
            pDirectCommands->transition(pResource, from, to);
        }

        void setDisplay(const render::Display& display) {
            pDirectCommands->setDisplay(display);
        }

        void setPipeline(render::PipelineState *pPipeline) {
            pDirectCommands->setPipelineState(pPipeline);
        }

        void setRenderTarget(RenderTargetAlloc::Index index, const math::float4& clear) {
            if (currentRenderTarget == index) return;
            currentRenderTarget = index;

            auto host = pRenderTargetAlloc->hostOffset(index);
            pDirectCommands->setRenderTarget(host);
            pDirectCommands->clearRenderTarget(host, clear);
        }

        void setRenderTarget(RenderTargetAlloc::Index index) {
            if (currentRenderTarget == index) return;
            currentRenderTarget = index;

            pDirectCommands->setRenderTarget(pRenderTargetAlloc->hostOffset(index));
        }

        void setShaderInput(ShaderResourceAlloc::Index index, UINT slot) {
            pDirectCommands->setShaderInput(pDataAlloc->deviceOffset(index), slot);
        }

        void drawIndexBuffer(render::IndexBuffer *pBuffer, size_t count) {
            pDirectCommands->setIndexBuffer(pBuffer);
            pDirectCommands->drawIndexBuffer(count);
        }

        void setVertexBuffer(render::VertexBuffer *pBuffer) {
            pDirectCommands->setVertexBuffer(pBuffer);
        }

        // copy commands

        void copyTexture(render::TextureBuffer *pDst, render::UploadBuffer *pSrc, const render::TextureInfo& info, std::span<const std::byte> data) {
            pCopyCommands->copyTexture(pDst, pSrc, info, data);
        }

        void copyBuffer(render::DeviceResource *pDst, render::UploadBuffer *pSrc) {
            pCopyCommands->copyBuffer(pDst, pSrc);
        }

    private:
        RenderContext(const RenderCreateInfo& createInfo);

        // state selection

        render::Adapter* selectAdapter() {
            return adapters[createInfo.adapterIndex];
        }

        // create data that depends on the context
        void createContextData();
        void destroyContextData();

        // create data that depends on the device
        void createDeviceData();
        void destroyDeviceData();

        void createHeaps();
        void destroyHeaps();

        // create data that depends on the present queue
        void createDisplayData();
        void destroyDisplayData();

        void createFrameData();
        void destroyFrameData();

        // rendering

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

        std::atomic_size_t copyFenceValue = 1;

        size_t frameIndex = 0;
        size_t directFenceValue = 1;
        render::Fence *pFence;

        std::vector<FrameData> frameData;

        // swapchain resolution dependant data

        render::DisplayQueue *pDisplayQueue;

        // heaps

        RenderTargetAlloc *pRenderTargetAlloc;
        ShaderResourceAlloc *pDataAlloc;

        // state
    public:
        bool fullscreen = false;
        RenderTargetAlloc::Index currentRenderTarget = RenderTargetAlloc::Index::eInvalid;
    };
}
