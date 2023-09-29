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
    struct DescriptorHeap;
    struct PipelineState;

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

    struct Device {
        // public interface

        DeviceQueue *createQueue();
        Commands *createCommands();
        DescriptorHeap *createRenderTargetHeap(UINT count);
        PipelineState *createPipelineState(const PipelineCreateInfo& createInfo);
        Fence *createFence();

        template<typename T>
        VertexBuffer *createVertexBuffer(std::span<const T> data) {
            return createVertexBuffer(data.data(), data.size_bytes(), sizeof(T));
        }

        VertexBuffer *createVertexBuffer(const void *pData, size_t length, size_t stride);

        // resource management

        void mapRenderTarget(HostHeapOffset handle, RenderTarget *pTarget);


        // module interface

        static Device *create(IDXGIAdapter4 *pAdapter);
        ~Device();

        ID3D12Device *getDevice() { return pDevice; }

    private:
        Device(ID3D12Device *pDevice, ID3D12InfoQueue1 *pInfoQueue)
            : pDevice(pDevice)
            , pInfoQueue(pInfoQueue)
        { }

        ID3D12Device *pDevice;
        ID3D12InfoQueue1 *pInfoQueue;
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

    // commands

    enum struct ResourceState {
        ePresent,
        eRenderTarget
    };

    struct Commands {
        // public interface

        void begin();
        void end();

        void transition(RenderTarget *pTarget, ResourceState from, ResourceState to);
        void clearRenderTarget(HostHeapOffset handle, math::float4 colour);

        // module interface

        static Commands *create(ID3D12GraphicsCommandList *pList, ID3D12CommandAllocator *pAllocator);
        ~Commands();

        ID3D12GraphicsCommandList *getCommandList() { return pList; }
        ID3D12CommandAllocator *getAllocator() { return pAllocator; }

    private:
        Commands(ID3D12GraphicsCommandList *pList, ID3D12CommandAllocator *pAllocator)
            : pList(pList)
            , pAllocator(pAllocator)
        { }

        ID3D12GraphicsCommandList *pList;
        ID3D12CommandAllocator *pAllocator;
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
        ~VertexBuffer();

    private:
        VertexBuffer(ID3D12Resource *pResource, D3D12_VERTEX_BUFFER_VIEW view)
            : pResource(pResource)
            , view(view)
        { }

        ID3D12Resource *pResource;
        D3D12_VERTEX_BUFFER_VIEW view;
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
