#pragma once

#include "engine/core/bitmap.h"
#include "engine/core/error.h"

#include "engine/log/service.h"

#include "engine/depot/image.h"

#include "engine/rhi/rhi.h"

namespace simcoe::render {
    // forward declarations

    struct Context;

    // external api types

    struct RenderCreateInfo {
        HWND hWindow;

        size_t adapterIndex = 0;
        UINT backBufferCount = 2;

        UINT displayWidth;
        UINT displayHeight;

        UINT renderWidth;
        UINT renderHeight;

        size_t rtvHeapSize = 16;
        size_t dsvHeapSize = 4;
        size_t srvHeapSize = 1024;
    };

    // helper types

    template<typename T>
    struct DescriptorAlloc {
        using Index = core::BitMap::Index;

        DescriptorAlloc(rhi::DescriptorHeap *pHeap, size_t size)
            : pHeap(pHeap)
            , allocator(size)
        { }

        ~DescriptorAlloc() {
            delete pHeap;
        }

        void reset() {
            allocator.reset();
        }

        Index alloc() {
            if (Index idx = allocator.alloc(); idx != Index::eInvalid) {
                return idx;
            }

            core::throwFatal("descriptor heap is full");
        }

        void release(Index index) {
            allocator.release(index);
        }

        rhi::HostHeapOffset hostOffset(Index index) const {
            return pHeap->hostOffset(size_t(index));
        }

        rhi::DeviceHeapOffset deviceOffset(Index index) const {
            return pHeap->deviceOffset(size_t(index));
        }

        rhi::DescriptorHeap *pHeap;
        core::BitMap allocator;
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

    ///
    /// render context, not thread safe
    ///
    struct Context {
        static constexpr math::float4 kClearColour = { 0.0f, 0.2f, 0.4f, 1.0f };
        static constexpr math::float4 kBlackClearColour = { 0.0f, 0.0f, 0.0f, 1.0f };

        static Context *create(const RenderCreateInfo& createInfo);

        ~Context();

        void beginDirect();
        void endDirect();

        void beginRender();
        void endRender();

        void beginCopy();
        void endCopy();

        void beginCompute();
        void endCompute();

        void waitForCopyQueue();
        void waitForDirectQueue();
        void waitForComputeQueue();

        // actions

        void changeFullscreen(bool bFullscreen);
        void changeDisplaySize(UINT width, UINT height);
        void changeRenderSize(UINT width, UINT height);
        void changeBackBufferCount(UINT count);
        void changeAdapter(size_t index);

        void removeDevice() { pDevice->remove(); }

        void resumeFromFault();
        void reportFaultInfo();

        // getters
        const RenderCreateInfo& getCreateInfo() const { return createInfo; }
        size_t getFrameIndex() const { return frameIndex; }

        std::vector<rhi::Adapter*> &getAdapters() { return adapters; }

        rhi::Device *getDevice() const { return pDevice; }
        rhi::Commands *getDirectCommands() const { return pDirectCommands; }

        ShaderResourceAlloc *getSrvHeap() { return pResourceAlloc; }
        RenderTargetAlloc *getRtvHeap() { return pRenderTargetAlloc; }
        DepthStencilAlloc *getDsvHeap() { return pDepthStencilAlloc; }

        rhi::RenderTarget *getRenderTarget(size_t index) { return pDisplayQueue->getRenderTarget(index); }
        rhi::TypeFormat getSwapChainFormat() const { return rhi::TypeFormat::eRGBA8; }
        rhi::TypeFormat getDepthFormat() const { return rhi::TypeFormat::eDepth32; }

        // create resources
        rhi::TextureBuffer *createTextureRenderTarget(const rhi::TextureInfo& info, const math::float4& clearColour) {
            return pDevice->createTextureRenderTarget(info, clearColour);
        }

        rhi::DepthBuffer *createDepthStencil(const rhi::TextureInfo& info) {
            return pDevice->createDepthStencil(info);
        }

        rhi::UniformBuffer *createUniformBuffer(size_t size) {
            return pDevice->createUniformBuffer(size);
        }

        rhi::PipelineState *createGraphicsPipeline(const rhi::GraphicsPipelineInfo& info) {
            return pDevice->createGraphicsPipeline(info);
        }

        rhi::PipelineState *createComputePipeline(const rhi::ComputePipelineInfo& info) {
            return pDevice->createComputePipeline(info);
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

        rhi::RwTextureBuffer *createRwTexture(const rhi::TextureInfo& info) {
            return pDevice->createRwTexture(info);
        }

        // heap allocators
        RenderTargetAlloc::Index mapRenderTarget(rhi::DeviceResource *pResource) {
            auto index = pRenderTargetAlloc->alloc();
            pDevice->mapRenderTarget(pRenderTargetAlloc->hostOffset(index), pResource, getSwapChainFormat());
            return index;
        }

        ShaderResourceAlloc::Index mapTexture(rhi::TextureBuffer *pResource, size_t mips = 1) {
            auto index = pResourceAlloc->alloc();
            rhi::TextureMapInfo info = {
                .handle = pResourceAlloc->hostOffset(index),
                .pTexture = pResource,

                .mipLevels = mips,
                .format = rhi::TypeFormat::eRGBA8,
            };

            pDevice->mapTexture(info);
            return index;
        }

        ShaderResourceAlloc::Index mapRwTexture(rhi::RwTextureBuffer *pResource, size_t mip) {
            auto index = pResourceAlloc->alloc();
            rhi::RwTextureMapInfo info = {
                .handle = pResourceAlloc->hostOffset(index),
                .pTexture = pResource,

                .mipSlice = mip,
                .format = rhi::TypeFormat::eRGBA8,
            };

            pDevice->mapRwTexture(info);
            return index;
        }

        ShaderResourceAlloc::Index mapUniform(rhi::UniformBuffer *pBuffer, size_t size) {
            auto index = pResourceAlloc->alloc();
            pDevice->mapUniform(pResourceAlloc->hostOffset(index), pBuffer, size);
            return index;
        }

        ShaderResourceAlloc::Index allocSrvIndex() {
            return pResourceAlloc->alloc();
        }

        DepthStencilAlloc::Index mapDepth(rhi::DepthBuffer *pResource) {
            auto index = pDepthStencilAlloc->alloc();
            pDevice->mapDepthStencil(pDepthStencilAlloc->hostOffset(index), pResource, getDepthFormat());
            return index;
        }

        // compute commands
        void setComputePipeline(rhi::PipelineState *pPipeline) {
            pComputeCommands->setComputePipeline(pPipeline);
        }

        void setComputeShaderInput(UINT slot, ShaderResourceAlloc::Index index) {
            pComputeCommands->setComputeShaderInput(slot, pResourceAlloc->deviceOffset(index));
        }

        void dispatchCompute(UINT x, UINT y, UINT z = 1) {
            pComputeCommands->dispatchCompute(x, y, z);
        }

        // commands
        void transition(rhi::DeviceResource *pResource, rhi::ResourceState from, rhi::ResourceState to) {
            pDirectCommands->transition(pResource, from, to);
        }

        void transition(std::span<const rhi::Transition> transitions) {
            pDirectCommands->transition(transitions);
        }

        void setDisplay(const rhi::Display& display) {
            pDirectCommands->setDisplay(display);
        }

        void setGraphicsPipeline(rhi::PipelineState *pPipeline) {
            pDirectCommands->setGraphicsPipeline(pPipeline);
        }

        // render and depth commands

        void setRenderTarget(RenderTargetAlloc::Index index) {
            if (currentRenderTarget == index) return;
            currentRenderTarget = index;

            pDirectCommands->setRenderTarget(pRenderTargetAlloc->hostOffset(index));
        }

        void setRenderAndDepth(RenderTargetAlloc::Index rtvIndex, DepthStencilAlloc::Index dsvIndex) {
            auto rtvHost = pRenderTargetAlloc->hostOffset(rtvIndex);
            auto dsvHost = pDepthStencilAlloc->hostOffset(dsvIndex);
            pDirectCommands->setRenderTarget(rtvHost, dsvHost);
        }

        void clearDepthStencil(DepthStencilAlloc::Index index, float depth, uint8_t stencil) {
            pDirectCommands->clearDepthStencil(pDepthStencilAlloc->hostOffset(index), depth, stencil);
        }

        void clearRenderTarget(RenderTargetAlloc::Index index, const math::float4& clear) {
            pDirectCommands->clearRenderTarget(pRenderTargetAlloc->hostOffset(index), clear);
        }

        // pipeline commands

        void setGraphicsShaderInput(UINT slot, ShaderResourceAlloc::Index index) {
            pDirectCommands->setGraphicsShaderInput(slot, pResourceAlloc->deviceOffset(index));
        }

        void drawIndexBuffer(rhi::IndexBuffer *pBuffer, size_t count) {
            pDirectCommands->setIndexBuffer(pBuffer);
            pDirectCommands->drawIndexBuffer(count);
        }

        void setVertexBuffer(rhi::VertexBuffer *pBuffer, rhi::Topology topology = rhi::Topology::eTriangleList) {
            pDirectCommands->setVertexBuffer(pBuffer, topology);
        }

        void setIndexBuffer(rhi::IndexBuffer *pBuffer) {
            pDirectCommands->setIndexBuffer(pBuffer);
        }

        void drawIndexed(size_t count) {
            pDirectCommands->drawIndexBuffer(count);
        }

        void draw(size_t count) {
            pDirectCommands->drawVertexBuffer(count);
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
            if (createInfo.adapterIndex >= adapters.size()) {
                createInfo.adapterIndex = 0;
            }

            auto *pAdapter = adapters[createInfo.adapterIndex];
            auto info = pAdapter->getInfo();
            LOG_INFO("selected adapter: {}", info.name);
            return pAdapter;
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
        rhi::Fence *pCopyFence;
        std::atomic_size_t copyFenceValue = 1;

        rhi::CommandMemory *pCopyAllocator;
        rhi::Commands *pCopyCommands;

        // device compute data

        rhi::DeviceQueue *pComputeQueue;
        rhi::Fence *pComputeFence;
        std::atomic_size_t computeFenceValue = 1;

        rhi::CommandMemory *pComputeAllocator;
        rhi::Commands *pComputeCommands;

        // frame data

        size_t frameIndex = 0;
        size_t directFenceValue = 1;
        rhi::Fence *pDirectFence;

        std::vector<FrameData> frameData;

        // swapchain resolution dependant data

        rhi::DisplayQueue *pDisplayQueue;

        // heaps

        RenderTargetAlloc *pRenderTargetAlloc;
        ShaderResourceAlloc *pResourceAlloc;
        DepthStencilAlloc *pDepthStencilAlloc;

        // state
    public:
        // modifiable state
        std::atomic_bool bAllowTearing = false;

        // info
        bool bReportedFullscreen = false;
        RenderTargetAlloc::Index currentRenderTarget = RenderTargetAlloc::Index::eInvalid;
    };
}
