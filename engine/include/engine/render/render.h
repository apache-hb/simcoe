#pragma once

#include "engine/assets/assets.h"
#include "engine/rhi/rhi.h"
#include "engine/memory/bitmap.h"

namespace simcoe::render {
    // forward declarations

    struct Context;

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

        DescriptorAlloc(rhi::DescriptorHeap *pHeap, size_t size)
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

        rhi::HostHeapOffset hostOffset(Index index) const {
            return pHeap->hostOffset(size_t(index));
        }

        rhi::DeviceHeapOffset deviceOffset(Index index) const {
            return pHeap->deviceOffset(size_t(index));
        }

        rhi::DescriptorHeap *pHeap;
        engine::BitMap mem;
    };

    struct RenderTargetHeap;
    struct ShaderDataHeap;
    struct DepthStencilHeap;

    using RenderTargetAlloc = DescriptorAlloc<RenderTargetHeap>;
    using ShaderResourceAlloc = DescriptorAlloc<ShaderDataHeap>;
    using DepthStencilAlloc = DescriptorAlloc<DepthStencilHeap>;

    struct FrameData {
        rhi::CommandMemory *pMemory;
    };

    struct Context {
        static constexpr math::float4 kClearColour = { 0.0f, 0.2f, 0.4f, 1.0f };
        static constexpr math::float4 kBlackClearColour = { 0.0f, 0.0f, 0.0f, 1.0f };

        static Context *create(const RenderCreateInfo& createInfo);

        ~Context();

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

        void changeFullscreen(bool bFullscreen);
        void changeDisplaySize(UINT width, UINT height);
        void changeRenderSize(UINT width, UINT height);
        void changeBackBufferCount(UINT count);
        void changeAdapter(size_t index);

        // getters
        const RenderCreateInfo& getCreateInfo() const { return createInfo; }
        size_t getFrameIndex() const { return frameIndex; }

        std::vector<rhi::Adapter*> &getAdapters() { return adapters; }

        rhi::Device *getDevice() const { return pDevice; }
        rhi::Commands *getDirectCommands() const { return pDirectCommands; }

        ShaderResourceAlloc *getSrvHeap() { return pDataAlloc; }
        RenderTargetAlloc *getRtvHeap() { return pRenderTargetAlloc; }

        rhi::RenderTarget *getRenderTarget(size_t index) { return pDisplayQueue->getRenderTarget(index); }

        // create resources
        rhi::TextureBuffer *createTextureRenderTarget(const rhi::TextureInfo& createInfo, const math::float4& clearColour) {
            return pDevice->createTextureRenderTarget(createInfo, clearColour);
        }

        rhi::UniformBuffer *createUniformBuffer(size_t size) {
            return pDevice->createUniformBuffer(size);
        }

        rhi::PipelineState *createPipelineState(const rhi::PipelineCreateInfo& createInfo) {
            return pDevice->createPipelineState(createInfo);
        }

        rhi::UploadBuffer *createUploadBuffer(const void *pData, size_t size) {
            return pDevice->createUploadBuffer(pData, size);
        }

        rhi::IndexBuffer *createIndexBuffer(size_t length, rhi::TypeFormat format) {
            return pDevice->createIndexBuffer(length, format);
        }

        rhi::VertexBuffer *createVertexBuffer(size_t length, size_t stride) {
            return pDevice->createVertexBuffer(length, stride);
        }

        rhi::UploadBuffer *createTextureUploadBuffer(const rhi::TextureInfo& info) {
            return pDevice->createTextureUploadBuffer(info);
        }

        rhi::TextureBuffer *createTexture(const rhi::TextureInfo& info) {
            return pDevice->createTexture(info);
        }

        // heap allocators
        RenderTargetAlloc::Index mapRenderTarget(rhi::DeviceResource *pResource) {
            auto index = pRenderTargetAlloc->alloc();
            pDevice->mapRenderTarget(pRenderTargetAlloc->hostOffset(index), pResource);
            return index;
        }

        ShaderResourceAlloc::Index mapTexture(rhi::TextureBuffer *pResource) {
            auto index = pDataAlloc->alloc();
            pDevice->mapTexture(pDataAlloc->hostOffset(index), pResource);
            return index;
        }

        ShaderResourceAlloc::Index mapUniform(rhi::UniformBuffer *pBuffer, size_t size) {
            auto index = pDataAlloc->alloc();
            pDevice->mapUniform(pDataAlloc->hostOffset(index), pBuffer, size);
            return index;
        }

        ShaderResourceAlloc::Index allocSrvIndex() {
            return pDataAlloc->alloc();
        }

        // commands
        void transition(rhi::DeviceResource *pResource, rhi::ResourceState from, rhi::ResourceState to) {
            pDirectCommands->transition(pResource, from, to);
        }

        void setDisplay(const rhi::Display& display) {
            pDirectCommands->setDisplay(display);
        }

        void setPipeline(rhi::PipelineState *pPipeline) {
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

        void drawIndexBuffer(rhi::IndexBuffer *pBuffer, size_t count) {
            pDirectCommands->setIndexBuffer(pBuffer);
            pDirectCommands->drawIndexBuffer(count);
        }

        void setVertexBuffer(rhi::VertexBuffer *pBuffer) {
            pDirectCommands->setVertexBuffer(pBuffer);
        }

        // copy commands

        void copyTexture(rhi::TextureBuffer *pDst, rhi::UploadBuffer *pSrc, const rhi::TextureInfo& info, std::span<const std::byte> data) {
            pCopyCommands->copyTexture(pDst, pSrc, info, data);
        }

        void copyBuffer(rhi::DeviceResource *pDst, rhi::UploadBuffer *pSrc) {
            pCopyCommands->copyBuffer(pDst, pSrc);
        }

    private:
        Context(const RenderCreateInfo& createInfo);

        // state selection

        rhi::Adapter* selectAdapter() {
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

        rhi::Context *pContext;
        std::vector<rhi::Adapter*> adapters;
        rhi::Device *pDevice;

        rhi::DeviceQueue *pDirectQueue;
        rhi::Commands *pDirectCommands;

        // device copy data

        rhi::DeviceQueue *pCopyQueue;

        rhi::CommandMemory *pCopyAllocator;
        rhi::Commands *pCopyCommands;

        std::atomic_size_t copyFenceValue = 1;

        size_t frameIndex = 0;
        size_t directFenceValue = 1;
        rhi::Fence *pFence;

        std::vector<FrameData> frameData;

        // swapchain resolution dependant data

        rhi::DisplayQueue *pDisplayQueue;

        // heaps

        RenderTargetAlloc *pRenderTargetAlloc;
        ShaderResourceAlloc *pDataAlloc;
        DepthStencilAlloc *pDepthStencilAlloc;

        // state
    public:
        // modifiable state
        bool bAllowTearing = false;

        // info
        bool bReportedFullscreen = false;
        RenderTargetAlloc::Index currentRenderTarget = RenderTargetAlloc::Index::eInvalid;
    };
}
