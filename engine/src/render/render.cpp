#include "engine/render/render.h"

#include "engine/engine.h"

#include <directx/d3dx12.h>

#include <stdexcept>

#include <wrl.h>

using Microsoft::WRL::ComPtr;

using namespace simcoe::render;

#define HR_CHECK(expr) \
    do { \
        HRESULT hr = (expr); \
        if (FAILED(hr)) { \
            simcoe::logError(#expr); \
            return nullptr; \
        } \
    } while (false)

static std::string narrow(std::wstring_view wstr) {
    std::string result(wstr.size() + 1, '\0');
    size_t size = result.size();

    errno_t err = wcstombs_s(&size, result.data(), result.size(), wstr.data(), wstr.size());
    if (err != 0) {
        return "";
    }

    result.resize(size - 1);
    return result;
}

static D3D12_CPU_DESCRIPTOR_HANDLE hostHandle(HostHeapOffset handle) {
    return { .ptr = size_t(handle) };
}

static D3D12_RESOURCE_STATES getResourceState(ResourceState state) {
    switch (state) {
    case ResourceState::ePresent: return D3D12_RESOURCE_STATE_PRESENT;
    case ResourceState::eRenderTarget: return D3D12_RESOURCE_STATE_RENDER_TARGET;
    default: throw std::runtime_error("invalid resource state");
    }
}

static DXGI_FORMAT getTypeFormat(TypeFormat fmt) {
    switch (fmt) {
    case TypeFormat::eFloat3: return DXGI_FORMAT_R32G32B32_FLOAT;
    case TypeFormat::eFloat4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
    default: throw std::runtime_error("invalid type format");
    }
}

// commands

void Commands::begin() {
    pAllocator->Reset();
    pList->Reset(pAllocator, nullptr);
}

void Commands::end() {
    pList->Close();
}

void Commands::transition(RenderTarget *pTarget, ResourceState from, ResourceState to) {
    D3D12_RESOURCE_BARRIER barrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition = {
            .pResource = pTarget->getResource(),
            .Subresource = 0,
            .StateBefore = getResourceState(from),
            .StateAfter = getResourceState(to),
        },
    };

    pList->ResourceBarrier(1, &barrier);
}

void Commands::clearRenderTarget(HostHeapOffset handle, math::float4 colour) {
    pList->ClearRenderTargetView(hostHandle(handle), colour.data(), 0, nullptr);
}

Commands *Commands::create(ID3D12GraphicsCommandList *pList, ID3D12CommandAllocator *pAllocator) {
    return new Commands(pList, pAllocator);
}

Commands::~Commands() {
    pList->Release();
    pAllocator->Release();
}

// device queue

void DeviceQueue::signal(Fence *pFence, size_t value) {
    pQueue->Signal(pFence->getFence(), value);
}

void DeviceQueue::execute(Commands *pCommands) {
    ID3D12CommandList *pList = pCommands->getCommandList();
    pQueue->ExecuteCommandLists(1, &pList);
}

DisplayQueue *DeviceQueue::createDisplayQueue(Context *pContext, const DisplayQueueCreateInfo& createInfo) {
    IDXGIFactory6 *pFactory = pContext->getFactory();
    BOOL tearing = FALSE;
    if (!SUCCEEDED(pFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearing, sizeof(tearing)))) {
        tearing = FALSE;
    }

    DXGI_SWAP_CHAIN_DESC1 desc = {
        .Width = createInfo.width,
        .Height = createInfo.height,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc = { .Count = 1 },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = createInfo.bufferCount,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .Flags = tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u
    };

    IDXGISwapChain1 *pSwapChain1 = nullptr;
    HR_CHECK(pFactory->CreateSwapChainForHwnd(getQueue(), createInfo.hWindow, &desc, nullptr, nullptr, &pSwapChain1));
    HR_CHECK(pFactory->MakeWindowAssociation(createInfo.hWindow, DXGI_MWA_NO_ALT_ENTER));

    IDXGISwapChain4 *pSwapChain = nullptr;
    HR_CHECK(pSwapChain1->QueryInterface(IID_PPV_ARGS(&pSwapChain)));

    return DisplayQueue::create(pSwapChain, tearing);
}

DeviceQueue *DeviceQueue::create(ID3D12CommandQueue *pQueue) {
    return new DeviceQueue(pQueue);
}

DeviceQueue::~DeviceQueue() {
    pQueue->Release();
}

// display queue

size_t DisplayQueue::getFrameIndex() {
    return pSwapChain->GetCurrentBackBufferIndex();
}

RenderTarget *DisplayQueue::getRenderTarget(UINT index) {
    ID3D12Resource *pResource = nullptr;
    HR_CHECK(pSwapChain->GetBuffer(index, IID_PPV_ARGS(&pResource)));

    return RenderTarget::create(pResource);
}

void DisplayQueue::present() {
    UINT flags = tearing ? DXGI_PRESENT_ALLOW_TEARING : 0u;

    pSwapChain->Present(0, flags);
}

DisplayQueue *DisplayQueue::create(IDXGISwapChain4 *pSwapChain, bool tearing) {
    return new DisplayQueue(pSwapChain, tearing);
}

DisplayQueue::~DisplayQueue() {
    pSwapChain->Release();
}

// device

DeviceQueue *Device::createQueue() {
    ID3D12CommandQueue *pQueue = nullptr;
    D3D12_COMMAND_QUEUE_DESC desc = {
        .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
    };

    HR_CHECK(pDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&pQueue)));

    return DeviceQueue::create(pQueue);
}

Commands *Device::createCommands() {
    ID3D12CommandAllocator *pAllocator = nullptr;
    HR_CHECK(pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pAllocator)));

    ID3D12GraphicsCommandList *pList = nullptr;
    HR_CHECK(pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pAllocator, nullptr, IID_PPV_ARGS(&pList)));

    HR_CHECK(pList->Close());

    return Commands::create(pList, pAllocator);
}

DescriptorHeap *Device::createRenderTargetHeap(UINT count) {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = count,
    };

    ID3D12DescriptorHeap *pHeap = nullptr;
    HR_CHECK(pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pHeap)));

    UINT descriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    return DescriptorHeap::create(pHeap, descriptorSize);
}

PipelineState *Device::createPipelineState(const PipelineCreateInfo& createInfo) {
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
    };

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HR_CHECK(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));

    ID3D12RootSignature *pSignature = nullptr;
    HR_CHECK(getDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&pSignature)));

    std::vector<D3D12_INPUT_ELEMENT_DESC> attributes;
    for (const auto& attribute : createInfo.attributes) {
        D3D12_INPUT_ELEMENT_DESC desc = {
            .SemanticName = attribute.name.data(),
            .SemanticIndex = 0,
            .Format = getTypeFormat(attribute.format),
            .InputSlot = 0,
            .AlignedByteOffset = UINT(attribute.offset),
            .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
        };

        attributes.push_back(desc);
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {
        .pRootSignature = pSignature,
        .VS = CD3DX12_SHADER_BYTECODE(createInfo.vertexShader.data(), createInfo.vertexShader.size()),
        .PS = CD3DX12_SHADER_BYTECODE(createInfo.vertexShader.data(), createInfo.vertexShader.size()),

        .BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
        .SampleMask = UINT_MAX,

        .RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
        .DepthStencilState = { .DepthEnable = false, .StencilEnable = false },
        .InputLayout = {  attributes.data(), UINT(attributes.size()) },
        .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .NumRenderTargets = 1,
        .RTVFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
        .SampleDesc = { .Count = 1 },
    };

    ID3D12PipelineState *pPipeline = nullptr;
    HR_CHECK(pDevice->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&pPipeline)));

    return PipelineState::create(pSignature, pPipeline);
}

Fence *Device::createFence() {
    ID3D12Fence *pFence = nullptr;
    HR_CHECK(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence)));

    HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!hEvent) {
        return nullptr;
    }

    return Fence::create(pFence, hEvent);
}

VertexBuffer *Device::createVertexBuffer(const void *pData, size_t length, size_t stride) {
    ID3D12Resource *pResource = nullptr;
    D3D12_HEAP_PROPERTIES heap = {
        .Type = D3D12_HEAP_TYPE_UPLOAD,
    };

    D3D12_RESOURCE_DESC desc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Width = length,
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = { .Count = 1 },
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE,
    };

    HR_CHECK(pDevice->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&pResource)));

    void *pMapped = nullptr;
    HR_CHECK(pResource->Map(0, nullptr, &pMapped));
    memcpy(pMapped, pData, length);
    pResource->Unmap(0, nullptr);

    D3D12_VERTEX_BUFFER_VIEW view = {
        .BufferLocation = pResource->GetGPUVirtualAddress(),
        .SizeInBytes = UINT(length),
        .StrideInBytes = UINT(stride),
    };

    return VertexBuffer::create(pResource, view);
}

void Device::mapRenderTarget(HostHeapOffset handle, RenderTarget *pTarget) {
    D3D12_RENDER_TARGET_VIEW_DESC desc = {
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
    };

    pDevice->CreateRenderTargetView(pTarget->getResource(), &desc, hostHandle(handle));
}

Device *Device::create(IDXGIAdapter4 *pAdapter) {
    ID3D12Device *pDevice = nullptr;
    HR_CHECK(D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDevice)));

    ID3D12InfoQueue1 *pInfoQueue = nullptr;
    if (HRESULT hr = pDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue)); SUCCEEDED(hr)) {
        simcoe::logInfo("enabling d3d12 debug layer");
    } else {
        simcoe::logWarn("failed to enable d3d12 debug layer");
    }

    return new Device(pDevice, pInfoQueue);
}

Device::~Device() {
    if (pInfoQueue) { pInfoQueue->Release(); }
    pDevice->Release();
}

// adapter

Device *Adapter::createDevice() {
    return Device::create(pAdapter);
}

AdapterInfo Adapter::getInfo() {
    AdapterInfo info = {
        .name = narrow(desc.Description),
        .type = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) ? AdapterType::eSoftware : AdapterType::eDiscrete,
    };

    return info;
}

Adapter *Adapter::create(IDXGIAdapter1 *pAdapter) {
    IDXGIAdapter4 *pAdapter4 = nullptr;
    HR_CHECK(pAdapter->QueryInterface(IID_PPV_ARGS(&pAdapter4)));

    DXGI_ADAPTER_DESC1 desc;
    HR_CHECK(pAdapter4->GetDesc1(&desc));

    return new Adapter(pAdapter4, desc);
}

Adapter::~Adapter() {
    pAdapter->Release();
}

// context

std::vector<Adapter*> Context::getAdapters() {
    std::vector<Adapter*> adapters;

    IDXGIAdapter1 *pAdapter = nullptr;
    for (UINT i = 0; pFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        adapters.push_back(Adapter::create(pAdapter));
    }

    return adapters;
}

Context *Context::create() {
    UINT flags = DXGI_CREATE_FACTORY_DEBUG;
    IDXGIFactory6 *pFactory = nullptr;
    HR_CHECK(CreateDXGIFactory2(flags, IID_PPV_ARGS(&pFactory)));

    IDXGIDebug1 *pDebug = nullptr;
    if (HRESULT hr = DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug)); SUCCEEDED(hr)) {
        simcoe::logInfo("enabling dxgi debug layer");
        pDebug->EnableLeakTrackingForThread();
    } else {
        simcoe::logWarn("failed to enable dxgi debug layer");
    }

    return new Context(pFactory, pDebug);
}

Context::~Context() {
    pFactory->Release();
    if (pDebug) {
        pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
        pDebug->Release();
    }
}

// descriptor heap

DeviceHeapOffset DescriptorHeap::deviceOffset(UINT index) {
    return DeviceHeapOffset(pHeap->GetGPUDescriptorHandleForHeapStart().ptr + index * descriptorSize);
}

HostHeapOffset DescriptorHeap::hostOffset(UINT index) {
    return HostHeapOffset(pHeap->GetCPUDescriptorHandleForHeapStart().ptr + index * descriptorSize);
}

DescriptorHeap *DescriptorHeap::create(ID3D12DescriptorHeap *pHeap, UINT descriptorSize) {
    return new DescriptorHeap(pHeap, descriptorSize);
}

DescriptorHeap::~DescriptorHeap() {
    pHeap->Release();
}

// pipeline state

PipelineState *PipelineState::create(ID3D12RootSignature *pRootSignature, ID3D12PipelineState *pState) {
    return new PipelineState(pRootSignature, pState);
}

// render target

RenderTarget *RenderTarget::create(ID3D12Resource *pResource) {
    return new RenderTarget(pResource);
}

RenderTarget::~RenderTarget() {
    pResource->Release();
}

// vertex buffer

VertexBuffer *VertexBuffer::create(ID3D12Resource *pResource, D3D12_VERTEX_BUFFER_VIEW view) {
    return new VertexBuffer(pResource, view);
}

// fence

size_t Fence::getValue() {
    return pFence->GetCompletedValue();
}

void Fence::wait(size_t value) {
    pFence->SetEventOnCompletion(value, hEvent);
    WaitForSingleObject(hEvent, INFINITE);
}

Fence *Fence::create(ID3D12Fence *pFence, HANDLE hEvent) {
    return new Fence(pFence, hEvent);
}

Fence::~Fence() {
    pFence->Release();
    CloseHandle(hEvent);
}

#define DLLEXPORT __declspec(dllexport)

// agility
extern "C" {
    // load up the agility sdk
    DLLEXPORT extern const UINT D3D12SDKVersion = 610;
    DLLEXPORT extern const char* D3D12SDKPath = ".\\";
}

extern "C" {
    // ask vendors to use the high performance gpu if we have one
    DLLEXPORT extern const DWORD NvOptimusEnablement = 0x00000001;
    DLLEXPORT extern int AmdPowerXpressRequestHighPerformance = 1;
}
