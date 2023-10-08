#pragma once

#include "engine/engine.h"

#include "engine/math/math.h"

#include <string>
#include <vector>
#include <span>

#include <directx/d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>

#define UNIFORM_ALIGN alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
#define UNIFORM_STRUCT struct UNIFORM_ALIGN

namespace simcoe::rhi {
    // forwards

    struct Context;
    struct Adapter;
    struct Device;

    struct DisplayQueue;
    struct DeviceQueue;

    struct Commands;

    struct DeviceResource;
    struct RenderTarget;
    struct VertexBuffer;
    struct IndexBuffer;
    struct TextureBuffer;
    struct UploadBuffer;
    struct UniformBuffer;

    struct DescriptorHeap;
    struct PipelineState;
    struct CommandMemory;

    struct Fence;

    enum struct DeviceHeapOffset : size_t { eInvalid = SIZE_MAX };
    enum struct HostHeapOffset : size_t { eInvalid = SIZE_MAX };

    // context

    template<typename T>
    struct Object {
        std::string getName() {
            UINT length = 0;
            pObject->GetPrivateData(WKPDID_D3DDebugObjectNameW, &length, nullptr);

            std::wstring name;
            name.resize(length);

            pObject->GetPrivateData(WKPDID_D3DDebugObjectNameW, &length, name.data());

            return util::narrow(name);
        }

        void setName(std::string_view name) {
            std::wstring wname = util::widen(name);

            pObject->SetPrivateData(WKPDID_D3DDebugObjectNameW, UINT(wname.size() * sizeof(wchar_t)), wname.data());
        }

    protected:
        Object(T *pObject)
            : pObject(pObject)
        { }

        ~Object() {
            pObject->Release();
        }

        T *get() { return pObject; }

    private:
        T *pObject;
    };

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

        eFloat2,
        eFloat3,
        eFloat4
    };

    enum struct InputVisibility {
        ePixel,
        eVertex
    };

    enum struct ResourceState {
        ePresent,
        eRenderTarget,
        eShaderResource,
        eCopyDest
    };

    struct VertexAttribute {
        std::string_view name;
        size_t offset;
        TypeFormat format;
    };

    struct InputSlot {
        InputVisibility visibility;
        size_t reg;
        bool isStatic;
    };

    struct SamplerSlot {
        InputVisibility visibility;
        size_t reg;
    };

    struct PipelineCreateInfo {
        std::vector<std::byte> vertexShader;
        std::vector<std::byte> pixelShader;

        std::vector<VertexAttribute> attributes;

        std::vector<InputSlot> textureInputs;
        std::vector<InputSlot> uniformInputs;

        std::vector<SamplerSlot> samplers;
    };

    enum struct PixelFormat {
        eRGBA8
    };

    struct TextureInfo {
        size_t width;
        size_t height;

        PixelFormat format;
    };

    enum struct CommandType {
        eDirect,
        eCopy
    };

    struct Device : Object<ID3D12Device4> {
        // public interface

        DeviceQueue *createQueue(CommandType type);
        Commands *createCommands(CommandType type, CommandMemory *pMemory);
        CommandMemory *createCommandMemory(CommandType type);

        DescriptorHeap *createRenderTargetHeap(UINT count);
        DescriptorHeap *createShaderDataHeap(UINT count);
        DescriptorHeap *createDepthStencilHeap(UINT count);

        PipelineState *createPipelineState(const PipelineCreateInfo& createInfo);
        Fence *createFence();

        // buffer creation

        VertexBuffer *createVertexBuffer(size_t length, size_t stride);
        IndexBuffer *createIndexBuffer(size_t length, TypeFormat fmt);
        UniformBuffer *createUniformBuffer(size_t length);

        TextureBuffer *createTextureRenderTarget(const TextureInfo& createInfo, const math::float4& clearColour);
        TextureBuffer *createTexture(const TextureInfo& createInfo);

        UploadBuffer *createUploadBuffer(const void *pData, size_t length);
        UploadBuffer *createTextureUploadBuffer(const TextureInfo& createInfo);

        // resource management

        void mapRenderTarget(HostHeapOffset handle, DeviceResource *pTarget);
        void mapUniform(HostHeapOffset handle, UniformBuffer *pUniform, size_t size);
        void mapTexture(HostHeapOffset handle, TextureBuffer *pTexture);

        // module interface

        static Device *create(IDXGIAdapter4 *pAdapter);
        ~Device();

        ID3D12Device4 *getDevice() { return Object::get(); }

    private:
        Device(ID3D12Device4 *pDevice, ID3D12InfoQueue1 *pInfoQueue, DWORD cookie, D3D_ROOT_SIGNATURE_VERSION version)
            : Object(pDevice)
            , pInfoQueue(pInfoQueue)
            , cookie(cookie)
            , rootSignatureVersion(version)
        { }

        ID3D12InfoQueue1 *pInfoQueue;
        DWORD cookie;
        D3D_ROOT_SIGNATURE_VERSION rootSignatureVersion;
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
        bool getFullscreenState();
        void setFullscreenState(bool fullscreen);

        void resizeBuffers(UINT bufferCount, UINT width, UINT height);

        void present(bool allowTearing);

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

    struct DeviceQueue : Object<ID3D12CommandQueue> {
        // public interface

        DisplayQueue *createDisplayQueue(Context *pContext, const DisplayQueueCreateInfo& createInfo);

        void signal(Fence *pFence, size_t value);
        void execute(Commands *pCommands);

        // module interface

        static DeviceQueue *create(ID3D12CommandQueue *pQueue);

        ID3D12CommandQueue *getQueue() { return Object::get(); }

    private:
        DeviceQueue(ID3D12CommandQueue *pQueue)
            : Object(pQueue)
        { }
    };

    // allocator

    struct CommandMemory : Object<ID3D12CommandAllocator> {
        // module interface

        static CommandMemory *create(ID3D12CommandAllocator *pAllocator);

        ID3D12CommandAllocator *getAllocator() { return Object::get(); }
    private:
        CommandMemory(ID3D12CommandAllocator *pAllocator)
            : Object(pAllocator)
        { }
    };

    // commands

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

    struct Commands : Object<ID3D12GraphicsCommandList> {
        // public interface

        void begin(CommandMemory *pMemory);
        void end();

        void transition(DeviceResource *pTarget, ResourceState from, ResourceState to);
        void clearRenderTarget(HostHeapOffset handle, math::float4 colour);

        void setDisplay(const Display& display);
        void setPipelineState(PipelineState *pState);
        void setHeap(DescriptorHeap *pHeap);
        void setShaderInput(DeviceHeapOffset handle, UINT reg);
        void setRenderTarget(HostHeapOffset handle);
        void setRenderTarget(HostHeapOffset rtvHandle, HostHeapOffset dsvHandle);
        void setVertexBuffer(VertexBuffer *pBuffer);
        void setIndexBuffer(IndexBuffer *pBuffer);

        void drawVertexBuffer(UINT count);
        void drawIndexBuffer(UINT count);

        void copyBuffer(DeviceResource *pDestination, UploadBuffer *pSource);
        void copyTexture(TextureBuffer *pDestination, UploadBuffer *pSource, const TextureInfo& info, std::span<const std::byte> data);

        // module interface

        static Commands *create(ID3D12GraphicsCommandList *pList);

        ID3D12GraphicsCommandList *getCommandList() { return Object::get(); }

    private:
        Commands(ID3D12GraphicsCommandList *pList)
            : Object(pList)
        { }
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

    // any resource

    struct DeviceResource : Object<ID3D12Resource> {
        ID3D12Resource *getResource() { return Object::get(); }

    protected:
        DeviceResource(ID3D12Resource *pResource)
            : Object(pResource)
        { }
    };

    // render target

    struct RenderTarget : DeviceResource {
        using DeviceResource::DeviceResource;
        static RenderTarget *create(ID3D12Resource *pResource);
    };

    struct VertexBuffer : DeviceResource {
        static VertexBuffer *create(ID3D12Resource *pResource, D3D12_VERTEX_BUFFER_VIEW view);

        D3D12_VERTEX_BUFFER_VIEW getView() { return view; }

    private:
        VertexBuffer(ID3D12Resource *pResource, D3D12_VERTEX_BUFFER_VIEW view)
            : DeviceResource(pResource)
            , view(view)
        { }

        D3D12_VERTEX_BUFFER_VIEW view;
    };

    struct IndexBuffer : DeviceResource {
        static IndexBuffer *create(ID3D12Resource *pResource, D3D12_INDEX_BUFFER_VIEW view);

        D3D12_INDEX_BUFFER_VIEW getView() { return view; }

    private:
        IndexBuffer(ID3D12Resource *pResource, D3D12_INDEX_BUFFER_VIEW view)
            : DeviceResource(pResource)
            , view(view)
        { }

        D3D12_INDEX_BUFFER_VIEW view;
    };

    struct TextureBuffer : DeviceResource {
        using DeviceResource::DeviceResource;
        static TextureBuffer *create(ID3D12Resource *pResource);
    };

    struct UploadBuffer : DeviceResource {
        using DeviceResource::DeviceResource;
        static UploadBuffer *create(ID3D12Resource *pResource);
    };

    struct UniformBuffer : DeviceResource {
        using DeviceResource::DeviceResource;

        void write(const void *pData, size_t length);

        static UniformBuffer *create(ID3D12Resource *pResource, void *pData);
        ~UniformBuffer();

    private:
        UniformBuffer(ID3D12Resource *pResource, void *pData)
            : DeviceResource(pResource)
            , pMapped(pData)
        { }

        void *pMapped;
    };

    // descriptor heap

    struct DescriptorHeap : Object<ID3D12DescriptorHeap> {
        DeviceHeapOffset deviceOffset(UINT index);
        HostHeapOffset hostOffset(UINT index);

        static DescriptorHeap *create(ID3D12DescriptorHeap *pHeap, UINT descriptorSize);

        ID3D12DescriptorHeap *getHeap() { return Object::get(); }

    private:
        DescriptorHeap(ID3D12DescriptorHeap *pHeap, UINT descriptorSize)
            : Object(pHeap)
            , descriptorSize(descriptorSize)
        { }

        UINT descriptorSize;
    };

    // fence

    struct Fence : Object<ID3D12Fence> {
        // public interface

        size_t getValue();
        void wait(size_t value);

        // module interface
        static Fence *create(ID3D12Fence *pFence, HANDLE hEvent);
        ~Fence();

        ID3D12Fence *getFence() { return Object::get(); }
    private:
        Fence(ID3D12Fence *pFence, HANDLE hEvent)
            : Object(pFence)
            , hEvent(hEvent)
        { }

        HANDLE hEvent;
    };
}
