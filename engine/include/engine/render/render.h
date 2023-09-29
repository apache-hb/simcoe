#pragma once

#include "engine/math/math.h"

#include <string>
#include <vector>
#include <span>

#include <directx/d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>

namespace simcoe::render {
    // forwards

    struct Context;
    struct Adapter;
    struct Device;

    struct DisplayQueue;
    struct DeviceQueue;

    struct Commands;

    struct RenderTarget;
    struct VertexBuffer;
    struct IndexBuffer;
    struct Texture;
    struct UploadBuffer;

    struct DescriptorHeap;
    struct PipelineState;
    struct CommandMemory;

    struct Fence;

    enum struct DeviceHeapOffset : size_t { eInvalid = SIZE_MAX };
    enum struct HostHeapOffset : size_t { eInvalid = SIZE_MAX };

    // context

    struct Context {
        // public interface

        std::vector<Adapter*> getAdapters();

        // module interface

        static Context *create();
        ~Context();

        IDXGIFactory6 *getFactory() { return pFactory; }

    private:
        Context(IDXGIFactory6 *pFactory, IDXGIDebug1 *pDebug)
            : pFactory(pFactory)
            , pDebug(pDebug)
        { }

        IDXGIFactory6 *pFactory;
        IDXGIDebug1 *pDebug = nullptr;
    };

    // adapter

    enum struct AdapterType {
        eDiscrete,
        eSoftware
    };

    struct AdapterInfo {
        std::string name;
        AdapterType type;
    };

    struct Adapter {
        Device *createDevice();
        AdapterInfo getInfo();

        static Adapter *create(IDXGIAdapter1 *pAdapter);
        ~Adapter();

        IDXGIAdapter4 *getAdapter() { return pAdapter; }

    private:
        Adapter(IDXGIAdapter4 *pAdapter, DXGI_ADAPTER_DESC1 desc)
            : pAdapter(pAdapter)
            , desc(desc)
        { }

        IDXGIAdapter4 *pAdapter;
        DXGI_ADAPTER_DESC1 desc;
    };

    // device

    enum struct TypeFormat {
        eUint16,
        eUint32,

        eFloat3,
        eFloat4
    };

    struct VertexAttribute {
        std::string_view name;
        size_t offset;
        TypeFormat format;
    };

    struct PipelineCreateInfo {
        std::vector<std::byte> vertexShader;
        std::vector<std::byte> pixelShader;

        std::vector<VertexAttribute> attributes;
    };

    enum struct PixelFormat {
        eRGBA8
    };

    struct TextureCreateInfo {
        size_t width;
        size_t height;

        PixelFormat format;

        std::vector<std::byte> data;
    };

    struct Device {
        // public interface

        DeviceQueue *createQueue();
        Commands *createCommands(CommandMemory *pMemory);
        CommandMemory *createCommandMemory();
        DescriptorHeap *createRenderTargetHeap(UINT count);
        PipelineState *createPipelineState(const PipelineCreateInfo& createInfo);
        Fence *createFence();

        template<typename T>
        VertexBuffer *createVertexBuffer(std::span<const T> data) {
            return createVertexBuffer(data.data(), data.size_bytes(), sizeof(T));
        }

        VertexBuffer *createVertexBuffer(const void *pData, size_t length, size_t stride);



        template<typename T>
        IndexBuffer *createIndexBuffer(std::span<const T> data, TypeFormat fmt) {
            return createIndexBuffer(data.data(), data.size_bytes(), sizeof(T), fmt);
        }

        IndexBuffer *createIndexBuffer(const void *pData, size_t length, size_t stride, TypeFormat fmt);



        Texture *createTexture(const TextureCreateInfo& createInfo);

        UploadBuffer *createUploadBuffer(size_t length);

        // resource management

        void mapRenderTarget(HostHeapOffset handle, RenderTarget *pTarget);
        void mapTexture(HostHeapOffset handle, Texture *pTexture);

        // module interface

        static Device *create(IDXGIAdapter4 *pAdapter);
        ~Device();

        ID3D12Device *getDevice() { return pDevice; }

    private:
        Device(ID3D12Device *pDevice, ID3D12InfoQueue1 *pInfoQueue, DWORD cookie)
            : pDevice(pDevice)
            , pInfoQueue(pInfoQueue)
            , cookie(cookie)
        { }

        ID3D12Device *pDevice;
        ID3D12InfoQueue1 *pInfoQueue;
        DWORD cookie;
    };

    // display

    struct DisplayQueueCreateInfo {
        HWND hWindow;

        UINT width;
        UINT height;

        UINT bufferCount;
    };

    struct DisplayQueue {
        // public interface
        RenderTarget *getRenderTarget(UINT index);
        size_t getFrameIndex();

        void present();

        // module interface

        static DisplayQueue *create(IDXGISwapChain4 *pSwapChain, bool tearing);
        ~DisplayQueue();

        IDXGISwapChain4 *getSwapChain() { return pSwapChain; }

    private:
        DisplayQueue(IDXGISwapChain4 *pSwapChain, bool tearing)
            : pSwapChain(pSwapChain)
            , tearing(tearing)
        { }

        IDXGISwapChain4 *pSwapChain;
        bool tearing;
    };

    // queue

    struct DeviceQueue {
        // public interface

        DisplayQueue *createDisplayQueue(Context *pContext, const DisplayQueueCreateInfo& createInfo);

        void signal(Fence *pFence, size_t value);
        void execute(Commands *pCommands);

        // module interface

        static DeviceQueue *create(ID3D12CommandQueue *pQueue);
        ~DeviceQueue();

        ID3D12CommandQueue *getQueue() { return pQueue; }

    private:
        DeviceQueue(ID3D12CommandQueue *pQueue)
            : pQueue(pQueue)
        { }

        ID3D12CommandQueue *pQueue;
    };

    // allocator

    struct CommandMemory {
        // module interface

        static CommandMemory *create(ID3D12CommandAllocator *pAllocator);
        ~CommandMemory();

        ID3D12CommandAllocator *getAllocator() { return pAllocator; }
    private:
        CommandMemory(ID3D12CommandAllocator *pAllocator)
            : pAllocator(pAllocator)
        { }

        ID3D12CommandAllocator *pAllocator;
    };

    // commands

    enum struct ResourceState {
        ePresent,
        eRenderTarget
    };

    struct Viewport {
        float x;
        float y;
        float width;
        float height;

        float minDepth;
        float maxDepth;
    };

    struct Scissor {
        LONG left;
        LONG top;
        LONG right;
        LONG bottom;
    };

    struct Display {
        Viewport viewport;
        Scissor scissor;
    };

    struct Commands {
        // public interface

        void begin(CommandMemory *pMemory);
        void end();

        void transition(RenderTarget *pTarget, ResourceState from, ResourceState to);
        void clearRenderTarget(HostHeapOffset handle, math::float4 colour);

        void setDisplay(const Display& display);
        void setPipelineState(PipelineState *pState);
        void setRenderTarget(HostHeapOffset handle);
        void setVertexBuffer(VertexBuffer *pBuffer);
        void setIndexBuffer(IndexBuffer *pBuffer);

        void drawVertexBuffer(UINT count);
        void drawIndexBuffer(UINT count);

        // module interface

        static Commands *create(ID3D12GraphicsCommandList *pList);
        ~Commands();

        ID3D12GraphicsCommandList *getCommandList() { return pList; }

    private:
        Commands(ID3D12GraphicsCommandList *pList)
            : pList(pList)
        { }

        ID3D12GraphicsCommandList *pList;
    };

    struct PipelineState {
        // module interface
        static PipelineState *create(ID3D12RootSignature *pRootSignature, ID3D12PipelineState *pState);
        ~PipelineState();

        ID3D12RootSignature *getRootSignature() { return pRootSignature; }
        ID3D12PipelineState *getState() { return pState; }
    private:
        PipelineState(ID3D12RootSignature *pRootSignature, ID3D12PipelineState *pState)
            : pRootSignature(pRootSignature)
            , pState(pState)
        { }

        ID3D12RootSignature *pRootSignature;
        ID3D12PipelineState *pState;
    };

    // render target

    struct RenderTarget {
        static RenderTarget *create(ID3D12Resource *pResource);

        ID3D12Resource *getResource() { return pResource; }
        ~RenderTarget();

    private:
        RenderTarget(ID3D12Resource *pResource)
            : pResource(pResource)
        { }

        ID3D12Resource *pResource;
    };

    struct VertexBuffer {
        static VertexBuffer *create(ID3D12Resource *pResource, D3D12_VERTEX_BUFFER_VIEW view);

        ID3D12Resource *getResource() { return pResource; }
        D3D12_VERTEX_BUFFER_VIEW getView() { return view; }
        ~VertexBuffer();

    private:
        VertexBuffer(ID3D12Resource *pResource, D3D12_VERTEX_BUFFER_VIEW view)
            : pResource(pResource)
            , view(view)
        { }

        ID3D12Resource *pResource;
        D3D12_VERTEX_BUFFER_VIEW view;
    };

    struct IndexBuffer {
        static IndexBuffer *create(ID3D12Resource *pResource, D3D12_INDEX_BUFFER_VIEW view);

        ID3D12Resource *getResource() { return pResource; }
        D3D12_INDEX_BUFFER_VIEW getView() { return view; }
        ~IndexBuffer();

    private:
        IndexBuffer(ID3D12Resource *pResource, D3D12_INDEX_BUFFER_VIEW view)
            : pResource(pResource)
            , view(view)
        { }

        ID3D12Resource *pResource;
        D3D12_INDEX_BUFFER_VIEW view;
    };

    // descriptor heap

    struct DescriptorHeap {
        DeviceHeapOffset deviceOffset(UINT index);
        HostHeapOffset hostOffset(UINT index);

        static DescriptorHeap *create(ID3D12DescriptorHeap *pHeap, UINT descriptorSize);
        ~DescriptorHeap();

    private:
        DescriptorHeap(ID3D12DescriptorHeap *pHeap, UINT descriptorSize)
            : pHeap(pHeap)
            , descriptorSize(descriptorSize)
        { }

        ID3D12DescriptorHeap *pHeap;
        UINT descriptorSize;
    };

    // fence

    struct Fence {
        // public interface

        size_t getValue();
        void wait(size_t value);

        // module interface
        static Fence *create(ID3D12Fence *pFence, HANDLE hEvent);
        ~Fence();

        ID3D12Fence *getFence() { return pFence; }
    private:
        Fence(ID3D12Fence *pFence, HANDLE hEvent)
            : pFence(pFence)
            , hEvent(hEvent)
        { }

        ID3D12Fence *pFence;
        HANDLE hEvent;
    };
}
