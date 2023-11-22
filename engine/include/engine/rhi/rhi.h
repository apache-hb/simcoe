#pragma once

#include "engine/math/math.h"
#include "engine/core/strings.h"

#include "engine/core/units.h"

#include <string>
#include <vector>
#include <span>
#include <atomic>
#include <unordered_map>

#include <directx/d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>

#define UNIFORM_ALIGN D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT
#define UNIFORM_BUFFER alignas(UNIFORM_ALIGN)

namespace simcoe::rhi {
    namespace units = simcoe::units;
    
    // forwards

    struct Context;
    struct Adapter;
    struct Device;

    struct DisplayQueue;
    struct DeviceQueue;

    struct Commands;

    struct Heap;
    struct DeviceResource;
    struct RenderTarget;
    struct VertexBuffer;
    struct IndexBuffer;
    struct TextureBuffer;
    struct RwTextureBuffer;
    struct DepthBuffer;
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

        void addRef() { pObject->AddRef(); }

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

    enum CreateFlags : int {
        eCreateNone = 0,
        eCreateDebug = (1 << 0),
        eCreateInfoQueue = (1 << 1),
        eCreateExtendedInfo = (1 << 2)
    };

    SM_ENUM_FLAGS(CreateFlags, CreateFlagsInner)

    struct Context {
        // public interface

        void reportLiveObjects();

        std::vector<Adapter*> getAdapters();

        Adapter *getWarpAdapter();
        Adapter *getLowPowerAdapter();
        Adapter *getFastestAdapter();

        // module interface

        static Context *create(CreateFlags flags);
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

        units::Memory videoMemory;
        units::Memory systemMemory;
        units::Memory sharedMemory;

        uint32_t vendorId;
        uint32_t deviceId;
        uint32_t subsystemId;
        uint32_t revision;
    };

    struct Adapter : Object<IDXGIAdapter4> {
        Device *createDevice(CreateFlags flags);
        AdapterInfo getInfo();

        static Adapter *create(IDXGIAdapter1 *pAdapter);

        IDXGIAdapter4 *getAdapter() { return get(); }

    private:
        Adapter(IDXGIAdapter4 *pAdapter, DXGI_ADAPTER_DESC1 desc)
            : Object(pAdapter)
            , desc(desc)
        { }

        DXGI_ADAPTER_DESC1 desc;
    };

    // device

    enum struct TypeFormat {
        eNone,

        eUint8x4,

        eUint16,
        eUint32,

        eDepth32,

        eFloat2,
        eFloat3,
        eFloat4,

        eRGBA8
    };

    DXGI_FORMAT getTypeFormat(TypeFormat format);

    enum struct InputVisibility {
        ePixel,
        eVertex,
        eCompute
    };

    enum struct ResourceState {
        eInvalid,

        ePresent,
        eRenderTarget,

        eTextureRead,
        eTextureWrite,
        eUniform,

        eVertexBuffer,
        eIndexBuffer,

        eDepthWrite,
        eCopyDest
    };

    std::string_view toString(ResourceState state);
    std::string_view toString(TypeFormat format);

    struct VertexAttribute {
        std::string_view name;
        size_t offset;
        TypeFormat format;
    };

    struct InputSlot {
        std::string_view name;
        InputVisibility visibility;
        size_t reg;
        bool isStatic;
    };

    struct SamplerSlot {
        InputVisibility visibility;
        size_t reg;
    };

    struct GraphicsPipelineInfo {
        std::vector<std::byte> vertexShader;
        std::vector<std::byte> pixelShader;

        std::vector<VertexAttribute> attributes;

        std::vector<InputSlot> textureInputs;
        std::vector<InputSlot> uniformInputs;

        std::vector<SamplerSlot> samplers;

        TypeFormat rtvFormat;

        bool depthEnable = false;
        TypeFormat dsvFormat = TypeFormat::eNone;
    };

    struct ComputePipelineInfo {
        std::vector<std::byte> computeShader;

        std::vector<InputSlot> textureInputs;
        std::vector<InputSlot> uniformInputs;
        std::vector<InputSlot> uavInputs;

        std::vector<SamplerSlot> samplers;
    };

    struct TextureInfo {
        size_t width;
        size_t height;

        TypeFormat format;
    };

    enum struct CommandType {
        eDirect,
        eCopy,
        eCompute
    };

    enum struct HeapType {
        eUpload,
        eDefault
    };

    struct TextureMapInfo {
        HostHeapOffset handle;
        TextureBuffer *pTexture;

        size_t mipLevels = 1;
        rhi::TypeFormat format;
    };

    struct RwTextureMapInfo {
        HostHeapOffset handle;
        RwTextureBuffer *pTexture;

        size_t mipSlice;
        rhi::TypeFormat format;
    };

    struct Device : Object<ID3D12Device4> {
        // public interface

        void remove();
        void reportFaultInfo();

        DeviceQueue *createQueue(CommandType type);
        Commands *createCommands(CommandType type, CommandMemory *pMemory);
        CommandMemory *createCommandMemory(CommandType type);

        DescriptorHeap *createRenderTargetHeap(UINT count);
        DescriptorHeap *createShaderDataHeap(UINT count);
        DescriptorHeap *createDepthStencilHeap(UINT count);

        PipelineState *createGraphicsPipeline(const GraphicsPipelineInfo& createInfo);
        PipelineState *createComputePipeline(const ComputePipelineInfo& createInfo);
        Fence *createFence();

        // buffer creation

        VertexBuffer *createVertexBuffer(size_t length, size_t stride, HeapType type = HeapType::eDefault);
        IndexBuffer *createIndexBuffer(size_t length, TypeFormat fmt, HeapType type = HeapType::eDefault);
        DepthBuffer *createDepthStencil(const TextureInfo& createInfo);
        UniformBuffer *createUniformBuffer(size_t length);

        TextureBuffer *createTextureRenderTarget(const TextureInfo& createInfo, const math::float4& clearColour);
        TextureBuffer *createTexture(const TextureInfo& createInfo);
        RwTextureBuffer *createRwTexture(const TextureInfo& createInfo);

        UploadBuffer *createUploadBuffer(const void *pData, size_t length);
        UploadBuffer *createTextureUploadBuffer(const TextureInfo& createInfo);

        // resource management

        void mapRenderTarget(HostHeapOffset handle, DeviceResource *pTarget, rhi::TypeFormat format);
        void mapDepthStencil(HostHeapOffset handle, DepthBuffer *pTarget, rhi::TypeFormat format);
        void mapUniform(HostHeapOffset handle, UniformBuffer *pUniform, size_t size);
        void mapTexture(const TextureMapInfo& info);
        void mapRwTexture(const RwTextureMapInfo& info);

        // module interface

        static Device *create(IDXGIAdapter4 *pAdapter, CreateFlags flags);
        ~Device();

        ID3D12Device4 *getDevice() { return Object::get(); }

        DWORD getCookie() const { return cookie; }

    private:
        Device(ID3D12Device4 *pDevice,
               ID3D12Debug *pDebug,
               ID3D12InfoQueue1 *pInfoQueue, DWORD cookie,
               CreateFlags createFlags,
               D3D_ROOT_SIGNATURE_VERSION version)
            : Object(pDevice)
            , pDebug(pDebug)
            , pInfoQueue(pInfoQueue)
            , cookie(cookie)
            , createFlags(createFlags)
            , rootSignatureVersion(version)
        { }

        // eCreateDebug
        ID3D12Debug *pDebug;

        // eCreateInfoQueue
        ID3D12InfoQueue1 *pInfoQueue;
        DWORD cookie;

        CreateFlags createFlags;
        D3D_ROOT_SIGNATURE_VERSION rootSignatureVersion;
    };

    // display

    struct DisplayQueueCreateInfo {
        HWND hWindow;

        UINT width;
        UINT height;

        UINT bufferCount;
        rhi::TypeFormat format;
    };

    struct DisplayQueue {
        // public interface
        RenderTarget *getRenderTarget(size_t index);
        size_t getFrameIndex();
        bool getFullscreenState();

        /**
         * @brief set the fullscreen state of the display
         *
         * @param fullscreen true to set fullscreen, false to set windowed
         * @return true if the state was changed
         * @return false if the state failed to change
         */
        bool setFullscreenState(bool fullscreen);

        void resizeBuffers(UINT bufferCount, UINT width, UINT height);

        void present(bool allowTearing, UINT syncInterval = 1);

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

        std::atomic_size_t failedFrames = 0;
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

    struct Transition {
        DeviceResource *pResource;
        ResourceState before;
        ResourceState after;
    };

    enum struct Topology {
        eTriangleList,
        eTriangleStrip
    };

    struct Commands : Object<ID3D12GraphicsCommandList> {
        // public interface

        // commom commands
        void begin(CommandMemory *pMemory);
        void end();
        void setHeap(DescriptorHeap *pHeap);

        void transition(DeviceResource *pTarget, ResourceState from, ResourceState to);
        void transition(std::span<const Transition> transitions);

        // graphics commands
        void setGraphicsPipeline(PipelineState *pState);
        void setGraphicsShaderInput(UINT reg, DeviceHeapOffset handle);

        void setRenderTarget(HostHeapOffset handle);
        void setRenderTarget(HostHeapOffset rtvHandle, HostHeapOffset dsvHandle);

        void setVertexBuffer(VertexBuffer *pBuffer, Topology topology = Topology::eTriangleList);
        void setIndexBuffer(IndexBuffer *pBuffer);

        void drawVertexBuffer(size_t count);
        void drawIndexBuffer(size_t count);

        void setDisplay(const Display& display);
        void clearRenderTarget(HostHeapOffset handle, math::float4 colour);
        void clearDepthStencil(HostHeapOffset handle, float depth, UINT8 stencil);

        // compute commands
        void setComputePipeline(PipelineState *pState);
        void setComputeShaderInput(UINT reg, DeviceHeapOffset handle);

        void dispatchCompute(UINT x, UINT y, UINT z);

        // copy commands
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
        using IndexMap = std::unordered_map<std::string_view, UINT>;
        // module interface
        static PipelineState *create(
            ID3D12RootSignature *pRootSignature,
            ID3D12PipelineState *pState,
            IndexMap textureInputs,
            IndexMap uniformInputs,
            IndexMap uavInputs
        );
        ~PipelineState();

        ID3D12RootSignature *getRootSignature() { return pRootSignature; }
        ID3D12PipelineState *getState() { return pState; }

        UINT getTextureInput(std::string_view name) { return textureInputs.at(name); }
        UINT getUniformInput(std::string_view name) { return uniformInputs.at(name); }
        UINT getUavInput(std::string_view name) { return uavInputs.at(name); }

        void setName(std::string_view name);

    private:
        PipelineState(ID3D12RootSignature *pRootSignature, ID3D12PipelineState *pState, IndexMap textureInputs, IndexMap uniformInputs, IndexMap uavInputs)
            : pRootSignature(pRootSignature)
            , pState(pState)
            , textureInputs(textureInputs)
            , uniformInputs(uniformInputs)
            , uavInputs(uavInputs)
        { }

        ID3D12RootSignature *pRootSignature;
        ID3D12PipelineState *pState;

        IndexMap textureInputs;
        IndexMap uniformInputs;
        IndexMap uavInputs;
    };

    // any resource

    struct DeviceResource : Object<ID3D12Resource> {
        ID3D12Resource *getResource() { return Object::get(); }

        void write(const void *pData, size_t length);

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

    struct RwTextureBuffer : DeviceResource {
        using DeviceResource::DeviceResource;
        static RwTextureBuffer *create(ID3D12Resource *pResource);
    };

    struct UploadBuffer : DeviceResource {
        using DeviceResource::DeviceResource;
        static UploadBuffer *create(ID3D12Resource *pResource);
    };

    struct DepthBuffer : DeviceResource {
        using DeviceResource::DeviceResource;
        static DepthBuffer *create(ID3D12Resource *pResource);
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
        DeviceHeapOffset deviceOffset(size_t index);
        HostHeapOffset hostOffset(size_t index);

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
