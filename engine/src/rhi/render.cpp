#include "engine/rhi/rhi.h"

#include "engine/os/system.h"
#include "engine/engine.h"

#include <directx/d3dx12.h>
#include <wrl.h>

#include <stdexcept>
#include <format>

using Microsoft::WRL::ComPtr;

namespace math = simcoe::math;
using namespace simcoe::rhi;

#define HR_CHECK(expr) \
    do { \
        HRESULT hr = (expr); \
        if (FAILED(hr)) { \
            simcoe::logError(#expr); \
            throw std::runtime_error(#expr); \
        } \
    } while (false)

static D3D_ROOT_SIGNATURE_VERSION getRootSigVersion(ID3D12Device4 *pDevice) {
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = { .HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1 };
    if (FAILED(pDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
        return D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    return featureData.HighestVersion;
}

static D3D12_CPU_DESCRIPTOR_HANDLE hostHandle(HostHeapOffset handle) {
    return { .ptr = size_t(handle) };
}

static D3D12_GPU_DESCRIPTOR_HANDLE deviceHandle(DeviceHeapOffset handle) {
    return { .ptr = size_t(handle) };
}

static D3D12_RESOURCE_STATES getResourceState(ResourceState state) {
    switch (state) {
    case ResourceState::ePresent: return D3D12_RESOURCE_STATE_PRESENT;
    case ResourceState::eRenderTarget: return D3D12_RESOURCE_STATE_RENDER_TARGET;
    case ResourceState::eShaderResource: return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    case ResourceState::eDepthWrite: return D3D12_RESOURCE_STATE_DEPTH_WRITE;
    case ResourceState::eCopyDest: return D3D12_RESOURCE_STATE_COPY_DEST;
    default: throw std::runtime_error("invalid resource state");
    }
}

static D3D12_COMMAND_LIST_TYPE getCommandType(CommandType type) {
    switch (type) {
    case CommandType::eDirect: return D3D12_COMMAND_LIST_TYPE_DIRECT;
    case CommandType::eCopy: return D3D12_COMMAND_LIST_TYPE_COPY;
    default: throw std::runtime_error("invalid command type");
    }
}

DXGI_FORMAT simcoe::rhi::getTypeFormat(TypeFormat fmt) {
    switch (fmt) {
    case TypeFormat::eNone: return DXGI_FORMAT_UNKNOWN;

    case TypeFormat::eDepth32: return DXGI_FORMAT_D32_FLOAT;

    case TypeFormat::eUint16: return DXGI_FORMAT_R16_UINT;
    case TypeFormat::eUint32: return DXGI_FORMAT_R32_UINT;

    case TypeFormat::eFloat2: return DXGI_FORMAT_R32G32_FLOAT;
    case TypeFormat::eFloat3: return DXGI_FORMAT_R32G32B32_FLOAT;
    case TypeFormat::eFloat4: return DXGI_FORMAT_R32G32B32A32_FLOAT;

    case TypeFormat::eRGBA8: return DXGI_FORMAT_R8G8B8A8_UNORM;
    default: throw std::runtime_error("invalid type format");
    }
}

static size_t getByteSize(TypeFormat fmt) {
    switch (fmt) {
    case TypeFormat::eUint16: return sizeof(uint16_t);
    case TypeFormat::eUint32: return sizeof(uint32_t);

    case TypeFormat::eFloat3: return sizeof(math::float3);
    case TypeFormat::eFloat4: return sizeof(math::float4);
    default: throw std::runtime_error("invalid type format");
    }
}

static size_t getPixelByteSize(TypeFormat fmt) {
    switch (fmt) {
    case TypeFormat::eRGBA8: return 4;
    default: throw std::runtime_error("invalid pixel format");
    }
}

static D3D12_SHADER_VISIBILITY getVisibility(InputVisibility visibility) {
    switch (visibility) {
    case InputVisibility::ePixel: return D3D12_SHADER_VISIBILITY_PIXEL;
    case InputVisibility::eVertex: return D3D12_SHADER_VISIBILITY_VERTEX;
    default: throw std::runtime_error("invalid shader visibility");
    }
}

static void setName(ID3D12Object *pObject, std::string_view name) {
    std::wstring it = simcoe::util::widen(name);
    pObject->SetName(it.c_str());
}

const char *severityToString(D3D12_MESSAGE_SEVERITY severity) {
    switch (severity) {
    case D3D12_MESSAGE_SEVERITY_CORRUPTION: return "CORRUPTION";
    case D3D12_MESSAGE_SEVERITY_ERROR: return "ERROR";
    case D3D12_MESSAGE_SEVERITY_WARNING: return "WARNING";
    case D3D12_MESSAGE_SEVERITY_INFO: return "INFO";
    case D3D12_MESSAGE_SEVERITY_MESSAGE: return "MESSAGE";
    default: return "UNKNOWN";
    }
}

const char *categoryToString(D3D12_MESSAGE_CATEGORY category) {
    switch (category) {
    case D3D12_MESSAGE_CATEGORY_APPLICATION_DEFINED: return "APPLICATION_DEFINED";
    case D3D12_MESSAGE_CATEGORY_MISCELLANEOUS: return "MISCELLANEOUS";
    case D3D12_MESSAGE_CATEGORY_INITIALIZATION: return "INITIALIZATION";
    case D3D12_MESSAGE_CATEGORY_CLEANUP: return "CLEANUP";
    case D3D12_MESSAGE_CATEGORY_COMPILATION: return "COMPILATION";
    case D3D12_MESSAGE_CATEGORY_STATE_CREATION: return "STATE_CREATION";
    case D3D12_MESSAGE_CATEGORY_STATE_SETTING: return "STATE_SETTING";
    case D3D12_MESSAGE_CATEGORY_STATE_GETTING: return "STATE_GETTING";
    case D3D12_MESSAGE_CATEGORY_RESOURCE_MANIPULATION: return "RESOURCE_MANIPULATION";
    case D3D12_MESSAGE_CATEGORY_EXECUTION: return "EXECUTION";
    case D3D12_MESSAGE_CATEGORY_SHADER: return "SHADER";
    default: return "UNKNOWN";
    }
}

static void debugCallback(D3D12_MESSAGE_CATEGORY category, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID id, LPCSTR desc, void *pUser) {
    const char *categoryStr = categoryToString(category);
    const char *severityStr = severityToString(severity);

    auto threadName = simcoe::getThreadName();

    simcoe::logInfo("thread: {}", threadName);
    switch (severity) {
    case D3D12_MESSAGE_SEVERITY_CORRUPTION:
    case D3D12_MESSAGE_SEVERITY_ERROR:
        simcoe::logError("{}: {} ({}): {}", categoryStr, severityStr, UINT(id), desc);
        break;

    case D3D12_MESSAGE_SEVERITY_WARNING:
        simcoe::logWarn("{}: {} ({}): {}", categoryStr, severityStr, UINT(id), desc);
        break;

    case D3D12_MESSAGE_SEVERITY_INFO:
    case D3D12_MESSAGE_SEVERITY_MESSAGE:
        simcoe::logInfo("{}: {} ({}): {}", categoryStr, severityStr, UINT(id), desc);
        break;

    default:
        simcoe::logInfo("{}: {} ({}): {}", categoryStr, severityStr, UINT(id), desc);
        break;
    }

    simcoe::logInfo("done");
}

// commands

void Commands::begin(CommandMemory *pMemory) {
    ID3D12CommandAllocator *pAllocator = pMemory->getAllocator();

    HR_CHECK(pAllocator->Reset());
    HR_CHECK(get()->Reset(pAllocator, nullptr));
}

void Commands::end() {
    HR_CHECK(get()->Close());
}

void Commands::transition(DeviceResource *pTarget, ResourceState from, ResourceState to) {
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

    get()->ResourceBarrier(1, &barrier);
}

void Commands::clearRenderTarget(HostHeapOffset handle, math::float4 colour) {
    get()->ClearRenderTargetView(hostHandle(handle), colour.data(), 0, nullptr);
}

void Commands::clearDepthStencil(HostHeapOffset handle, float depth, UINT8 stencil) {
    get()->ClearDepthStencilView(hostHandle(handle), D3D12_CLEAR_FLAG_DEPTH, depth, stencil, 0, nullptr);
}

void Commands::setDisplay(const Display& display) {
    const auto& [viewport, scissor] = display;
    D3D12_VIEWPORT v = {
        .TopLeftX = viewport.x,
        .TopLeftY = viewport.y,
        .Width = viewport.width,
        .Height = viewport.height,
        .MinDepth = viewport.minDepth,
        .MaxDepth = viewport.maxDepth,
    };

    D3D12_RECT s = {
        .left = scissor.left,
        .top = scissor.top,
        .right = scissor.right,
        .bottom = scissor.bottom,
    };

    get()->RSSetViewports(1, &v);
    get()->RSSetScissorRects(1, &s);
}

void Commands::setPipelineState(PipelineState *pState) {
    get()->SetGraphicsRootSignature(pState->getRootSignature());
    get()->SetPipelineState(pState->getState());
}

void Commands::setHeap(DescriptorHeap *pHeap) {
    ID3D12DescriptorHeap *pDescriptorHeap = pHeap->getHeap();
    get()->SetDescriptorHeaps(1, &pDescriptorHeap);
}

void Commands::setShaderInput(DeviceHeapOffset handle, UINT reg) {
    get()->SetGraphicsRootDescriptorTable(reg, deviceHandle(handle));
}

void Commands::setRenderTarget(HostHeapOffset handle) {
    D3D12_CPU_DESCRIPTOR_HANDLE handles[] = { hostHandle(handle) };
    get()->OMSetRenderTargets(1, handles, FALSE, nullptr);
}

void Commands::setRenderTarget(HostHeapOffset rtvHandle, HostHeapOffset dsvHandle) {
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[] = { hostHandle(rtvHandle) };
    D3D12_CPU_DESCRIPTOR_HANDLE innerDsvHandle = hostHandle(dsvHandle);
    get()->OMSetRenderTargets(1, rtvHandles, FALSE, &innerDsvHandle);
}

void Commands::setVertexBuffer(VertexBuffer *pBuffer) {
    get()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D12_VERTEX_BUFFER_VIEW views[] = { pBuffer->getView() };
    get()->IASetVertexBuffers(0, 1, views);
}

void Commands::setIndexBuffer(IndexBuffer *pBuffer) {
    D3D12_INDEX_BUFFER_VIEW view = pBuffer->getView();
    get()->IASetIndexBuffer(&view);
}

void Commands::drawVertexBuffer(UINT count) {
    get()->DrawInstanced(count, 1, 0, 0);
}

void Commands::drawIndexBuffer(UINT count) {
    get()->DrawIndexedInstanced(count, 1, 0, 0, 0);
}

void Commands::copyBuffer(DeviceResource *pDestination, UploadBuffer *pSource) {
    get()->CopyResource(pDestination->getResource(), pSource->getResource());
}

void Commands::copyTexture(TextureBuffer *pDestination, UploadBuffer *pSource, const TextureInfo& info, std::span<const std::byte> data) {
    LONG_PTR rowPitch = info.width * getPixelByteSize(info.format);
    LONG_PTR slicePitch = rowPitch * info.height;

    D3D12_SUBRESOURCE_DATA update = {
        .pData = data.data(),
        .RowPitch = rowPitch,
        .SlicePitch = slicePitch
    };

    UpdateSubresources(get(), pDestination->getResource(), pSource->getResource(), 0, 0, 1, &update);
}

Commands *Commands::create(ID3D12GraphicsCommandList *pList) {
    return new Commands(pList);
}

// command memory

CommandMemory *CommandMemory::create(ID3D12CommandAllocator *pAllocator) {
    return new CommandMemory(pAllocator);
}

// device queue

void DeviceQueue::signal(Fence *pFence, size_t value) {
    HR_CHECK(get()->Signal(pFence->getFence(), value));
}

void DeviceQueue::execute(Commands *pCommands) {
    ID3D12CommandList *pList = pCommands->getCommandList();
    get()->ExecuteCommandLists(1, &pList);
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
        .Format = getTypeFormat(createInfo.format),
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

    // release the swap chain 1 interface
    pSwapChain1->Release();

    return DisplayQueue::create(pSwapChain, tearing);
}

DeviceQueue *DeviceQueue::create(ID3D12CommandQueue *pQueue) {
    return new DeviceQueue(pQueue);
}

// display queue

size_t DisplayQueue::getFrameIndex() {
    return pSwapChain->GetCurrentBackBufferIndex();
}

bool DisplayQueue::getFullscreenState() {
    BOOL fullscreen = FALSE;
    HR_CHECK(pSwapChain->GetFullscreenState(&fullscreen, nullptr));

    return fullscreen;
}

void DisplayQueue::setFullscreenState(bool fullscreen) {
    HR_CHECK(pSwapChain->SetFullscreenState(fullscreen, nullptr));
}

void DisplayQueue::resizeBuffers(UINT bufferCount, UINT width, UINT height) {
    DXGI_SWAP_CHAIN_DESC desc = {};
    pSwapChain->GetDesc(&desc);

    HR_CHECK(pSwapChain->ResizeBuffers(bufferCount, width, height, desc.BufferDesc.Format, desc.Flags));
}

RenderTarget *DisplayQueue::getRenderTarget(UINT index) {
    ID3D12Resource *pResource = nullptr;
    HR_CHECK(pSwapChain->GetBuffer(index, IID_PPV_ARGS(&pResource)));

    return RenderTarget::create(pResource);
}

void DisplayQueue::present(bool allowTearing) {
    UINT flags = (tearing && allowTearing) ? DXGI_PRESENT_ALLOW_TEARING : 0u;

    HR_CHECK(pSwapChain->Present(0, flags));
}

DisplayQueue *DisplayQueue::create(IDXGISwapChain4 *pSwapChain, bool tearing) {
    return new DisplayQueue(pSwapChain, tearing);
}

DisplayQueue::~DisplayQueue() {
    pSwapChain->Release();
}

// device

void Device::remove() {
    ID3D12Device5 *pDevice5 = nullptr;
    if (HRESULT hr = get()->QueryInterface(IID_PPV_ARGS(&pDevice5)); SUCCEEDED(hr)) {
        simcoe::logInfo("removing device");
        pDevice5->RemoveDevice();
        pDevice5->Release();
    } else {
        simcoe::logWarn("failed to retrieve ID3D12Device5 interface (hr={:x})", unsigned(hr));
    }
}

void Device::reportFaultInfo() {
    HRESULT hr = get()->GetDeviceRemovedReason();

    if (!(createFlags & eCreateExtendedInfo)) {
        simcoe::logInfo("device removed (hr={:x})", unsigned(hr));
        return;
    }

    ComPtr<ID3D12DeviceRemovedExtendedData> pData = nullptr;
    if (HRESULT hr = get()->QueryInterface(IID_PPV_ARGS(&pData)); FAILED(hr)) {
        simcoe::logWarn("failed to retrieve ID3D12DeviceRemovedExtendedData interface ({})", getErrorName(hr));
        return;
    }

    D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT breadOutput = {};

    if (HRESULT hr = pData->GetAutoBreadcrumbsOutput(&breadOutput); FAILED(hr)) {
        simcoe::logWarn("failed to retrieve auto breadcrumbs ({})", getErrorName(hr));
        return;
    }

    simcoe::logInfo("auto breadcrumbs:");
    const D3D12_AUTO_BREADCRUMB_NODE *pNode = breadOutput.pHeadAutoBreadcrumbNode;
    while (pNode != nullptr) {
        simcoe::logInfo("  objects: (queue={}, list={})", pNode->pCommandQueueDebugNameA, pNode->pCommandListDebugNameA);
        simcoe::logInfo("  count: {}", pNode->BreadcrumbCount);
        for (UINT32 i = 0; i < pNode->BreadcrumbCount; i++) {
            const D3D12_AUTO_BREADCRUMB_OP &op = pNode->pCommandHistory[i];
            simcoe::logInfo("    op[{}]: {}", int(op));
        }
        pNode = pNode->pNext;
    }

    D3D12_DRED_PAGE_FAULT_OUTPUT faultOutput = {};

    if (HRESULT hr = pData->GetPageFaultAllocationOutput(&faultOutput); FAILED(hr)) {
        simcoe::logWarn("failed to retrieve page fault allocation ({})", getErrorName(hr));
        return;
    }

    simcoe::logInfo("page fault at 0x{:X}", faultOutput.PageFaultVA);
}

DeviceQueue *Device::createQueue(CommandType type) {
    ID3D12CommandQueue *pQueue = nullptr;
    D3D12_COMMAND_QUEUE_DESC desc = {
        .Type = getCommandType(type),
    };

    HR_CHECK(get()->CreateCommandQueue(&desc, IID_PPV_ARGS(&pQueue)));

    return DeviceQueue::create(pQueue);
}

Commands *Device::createCommands(CommandType type, CommandMemory *pMemory) {
    ID3D12CommandAllocator *pAllocator = pMemory->getAllocator();

    ID3D12GraphicsCommandList *pList = nullptr;
    HR_CHECK(get()->CreateCommandList(0, getCommandType(type), pAllocator, nullptr, IID_PPV_ARGS(&pList)));

    HR_CHECK(pList->Close());

    return Commands::create(pList);
}

CommandMemory *Device::createCommandMemory(CommandType type) {
    ID3D12CommandAllocator *pAllocator = nullptr;
    HR_CHECK(get()->CreateCommandAllocator(getCommandType(type), IID_PPV_ARGS(&pAllocator)));

    return CommandMemory::create(pAllocator);
}

DescriptorHeap *Device::createRenderTargetHeap(UINT count) {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = count,
    };

    ID3D12DescriptorHeap *pHeap = nullptr;
    HR_CHECK(get()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pHeap)));

    UINT descriptorSize = get()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    return DescriptorHeap::create(pHeap, descriptorSize);
}

DescriptorHeap *Device::createShaderDataHeap(UINT count) {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = count,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
    };

    ID3D12DescriptorHeap *pHeap = nullptr;
    HR_CHECK(get()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pHeap)));

    UINT descriptorSize = get()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    return DescriptorHeap::create(pHeap, descriptorSize);
}

DescriptorHeap *Device::createDepthStencilHeap(UINT count) {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        .NumDescriptors = count,
    };

    ID3D12DescriptorHeap *pHeap = nullptr;
    HR_CHECK(get()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pHeap)));

    UINT descriptorSize = get()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    return DescriptorHeap::create(pHeap, descriptorSize);
}

static CD3DX12_DESCRIPTOR_RANGE1 createRange(D3D12_DESCRIPTOR_RANGE_TYPE type, const InputSlot& slot) {
    CD3DX12_DESCRIPTOR_RANGE1 range;
    auto dataFlag = slot.isStatic ? D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC : D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
    range.Init(type, 1, slot.reg, 0, dataFlag, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);
    return range;
}

PipelineState *Device::createPipelineState(const PipelineCreateInfo& createInfo) {
    PipelineState::IndexMap textureIndices;
    PipelineState::IndexMap uniformIndices;

    // create root parameters
    std::vector<D3D12_ROOT_PARAMETER1> parameters;
    auto addParam = [&](const D3D12_ROOT_PARAMETER1& param) {
        parameters.push_back(param);
        return parameters.size() - 1;
    };

    // first check texture inputs
    size_t textureInputs = createInfo.textureInputs.size();
    std::unique_ptr<D3D12_DESCRIPTOR_RANGE1[]> pTextureRanges{new D3D12_DESCRIPTOR_RANGE1[textureInputs]};
    for (size_t i = 0; i < textureInputs; i++) {
        const auto& input = createInfo.textureInputs[i];

        pTextureRanges[i] = createRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, input);

        CD3DX12_ROOT_PARAMETER1 parameter;
        parameter.InitAsDescriptorTable(
            1, &pTextureRanges[i],
            getVisibility(input.visibility)
        );
        textureIndices[input.name] = addParam(parameter);
    }

    // then uniforms
    size_t uniformInputs = createInfo.uniformInputs.size();
    std::unique_ptr<D3D12_DESCRIPTOR_RANGE1[]> pUniformRanges{new D3D12_DESCRIPTOR_RANGE1[uniformInputs]};
    for (size_t i = 0; i < uniformInputs; i++) {
        const auto& input = createInfo.uniformInputs[i];

        pUniformRanges[i] = createRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, input);

        CD3DX12_ROOT_PARAMETER1 parameter;
        parameter.InitAsDescriptorTable(
            1, &pUniformRanges[i],
            getVisibility(input.visibility)
        );
        uniformIndices[input.name] = addParam(parameter);
    }

    // create samplers

    std::vector<D3D12_STATIC_SAMPLER_DESC> samplerDescs;
    for (const auto& sampler : createInfo.samplers) {
        D3D12_STATIC_SAMPLER_DESC desc = {
            .Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            .AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
            .AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
            .AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
            .MipLODBias = 0,
            .MaxAnisotropy = 0,
            .ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
            .BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
            .MinLOD = 0,
            .MaxLOD = D3D12_FLOAT32_MAX,
            .ShaderRegister = UINT(sampler.reg),
            .RegisterSpace = 0,
            .ShaderVisibility = getVisibility(sampler.visibility),
        };

        samplerDescs.push_back(desc);
    }

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(
        UINT(parameters.size()), parameters.data(),
        UINT(samplerDescs.size()), samplerDescs.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    if (HRESULT hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, rootSignatureVersion, &signature, &error); FAILED(hr)) {
        simcoe::logError("Failed to serialize root signature: {:X}", unsigned(hr));

        std::string_view msg{static_cast<LPCSTR>(error->GetBufferPointer()), error->GetBufferSize() / sizeof(char)};
        simcoe::logError("{}", msg);

        return nullptr;
    }

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

    const auto& vs = createInfo.vertexShader;
    const auto& ps = createInfo.pixelShader;

    D3D12_DEPTH_STENCIL_DESC dsvDesc = createInfo.depthEnable
        ? CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT)
        : D3D12_DEPTH_STENCIL_DESC{ .DepthEnable = false, .StencilEnable = false };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {
        .pRootSignature = pSignature,
        .VS = CD3DX12_SHADER_BYTECODE(vs.data(), vs.size()),
        .PS = CD3DX12_SHADER_BYTECODE(ps.data(), ps.size()),

        .BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
        .SampleMask = UINT_MAX,

        .RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
        .DepthStencilState = dsvDesc,
        .InputLayout = { attributes.data(), UINT(attributes.size()) },
        .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .NumRenderTargets = 1,
        .RTVFormats = { getTypeFormat(createInfo.rtvFormat) },
        .DSVFormat = getTypeFormat(createInfo.dsvFormat),
        .SampleDesc = { .Count = 1 },
    };

    ID3D12PipelineState *pPipeline = nullptr;
    HR_CHECK(get()->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&pPipeline)));

    return PipelineState::create(pSignature, pPipeline, textureIndices, uniformIndices);
}

Fence *Device::createFence() {
    ID3D12Fence *pFence = nullptr;
    HR_CHECK(get()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence)));

    HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!hEvent) {
        pFence->Release();
        return nullptr;
    }

    return Fence::create(pFence, hEvent);
}

VertexBuffer *Device::createVertexBuffer(size_t length, size_t stride) {
    size_t size = length * stride;
    ID3D12Resource *pResource = nullptr;

    D3D12_HEAP_PROPERTIES heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size);

    HR_CHECK(get()->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr, IID_PPV_ARGS(&pResource)
    ));

    D3D12_VERTEX_BUFFER_VIEW view = {
        .BufferLocation = pResource->GetGPUVirtualAddress(),
        .SizeInBytes = UINT(size),
        .StrideInBytes = UINT(stride),
    };

    return VertexBuffer::create(pResource, view);
}

IndexBuffer *Device::createIndexBuffer(size_t length, TypeFormat fmt) {
    ID3D12Resource *pResource = nullptr;

    size_t size = length * getByteSize(fmt);

    D3D12_HEAP_PROPERTIES heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size);

    HR_CHECK(get()->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr, IID_PPV_ARGS(&pResource)
    ));

    D3D12_INDEX_BUFFER_VIEW view = {
        .BufferLocation = pResource->GetGPUVirtualAddress(),
        .SizeInBytes = UINT(size),
        .Format = getTypeFormat(fmt)
    };

    return IndexBuffer::create(pResource, view);
}

DepthBuffer *Device::createDepthStencil(const TextureInfo& createInfo) {
    DXGI_FORMAT format = getTypeFormat(createInfo.format);
    D3D12_CLEAR_VALUE clear = {
        .Format = format,
        .DepthStencil = { 1.0f, 0 },
    };

    ID3D12Resource *pResource = nullptr;

    D3D12_HEAP_PROPERTIES heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC desc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Width = UINT(createInfo.width),
        .Height = UINT(createInfo.height),
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = format,
        .SampleDesc = { .Count = 1 },
        .Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
    };

    HR_CHECK(get()->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clear, IID_PPV_ARGS(&pResource)
    ));

    return DepthBuffer::create(pResource);
}

UniformBuffer *Device::createUniformBuffer(size_t length) {
    ID3D12Resource *pResource = nullptr;

    D3D12_HEAP_PROPERTIES heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(length);

    HR_CHECK(get()->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&pResource)
    ));

    // map the buffer
    void *pMapped = nullptr;
    HR_CHECK(pResource->Map(0, nullptr, &pMapped));

    return UniformBuffer::create(pResource, pMapped);
}

TextureBuffer *Device::createTextureRenderTarget(const TextureInfo& createInfo, const math::float4& clearColour) {
    DXGI_FORMAT format = getTypeFormat(createInfo.format);
    D3D12_CLEAR_VALUE clear = {
        .Format = format,
        .Color = { clearColour.x, clearColour.y, clearColour.z, clearColour.w },
    };

    ID3D12Resource *pResource = nullptr;

    D3D12_HEAP_PROPERTIES heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC desc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Width = UINT(createInfo.width),
        .Height = UINT(createInfo.height),
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = format,
        .SampleDesc = { .Count = 1 },
        .Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
    };

    HR_CHECK(get()->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clear, IID_PPV_ARGS(&pResource)
    ));

    return TextureBuffer::create(pResource);
}

TextureBuffer *Device::createTexture(const TextureInfo& createInfo) {
    ID3D12Resource *pResource = nullptr;

    D3D12_HEAP_PROPERTIES heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(
        getTypeFormat(createInfo.format),
        UINT(createInfo.width), UINT(createInfo.height)
    );

    HR_CHECK(get()->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr, IID_PPV_ARGS(&pResource)
    ));

    return TextureBuffer::create(pResource);
}

UploadBuffer *Device::createUploadBuffer(const void *pData, size_t length) {
    ID3D12Resource *pResource = nullptr;
    D3D12_HEAP_PROPERTIES heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(length);

    HR_CHECK(get()->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE,
        &desc, D3D12_RESOURCE_STATE_COMMON,
        nullptr, IID_PPV_ARGS(&pResource)
    ));

    void *pMapped = nullptr;
    HR_CHECK(pResource->Map(0, nullptr, &pMapped));
    memcpy(pMapped, pData, length);
    pResource->Unmap(0, nullptr);

    return UploadBuffer::create(pResource);
}

UploadBuffer *Device::createTextureUploadBuffer(const TextureInfo& createInfo) {
    size_t size = createInfo.width * createInfo.height * getPixelByteSize(createInfo.format);
    ID3D12Resource *pResource = nullptr;

    D3D12_HEAP_PROPERTIES heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(size);

    HR_CHECK(get()->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE,
        &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&pResource)
    ));

    return UploadBuffer::create(pResource);
}

void Device::mapRenderTarget(HostHeapOffset handle, DeviceResource *pTarget, rhi::TypeFormat format) {
    D3D12_RENDER_TARGET_VIEW_DESC desc = {
        .Format = getTypeFormat(format),
        .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
    };

    get()->CreateRenderTargetView(pTarget->getResource(), &desc, hostHandle(handle));
}

void Device::mapDepthStencil(HostHeapOffset handle, DepthBuffer *pTarget, rhi::TypeFormat format) {
    D3D12_DEPTH_STENCIL_VIEW_DESC desc = {
        .Format = getTypeFormat(format),
        .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
    };

    get()->CreateDepthStencilView(pTarget->getResource(), &desc, hostHandle(handle));
}

void Device::mapUniform(HostHeapOffset handle, UniformBuffer *pUniform, size_t size) {
    D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {
        .BufferLocation = pUniform->getResource()->GetGPUVirtualAddress(),
        .SizeInBytes = UINT(size),
    };

    get()->CreateConstantBufferView(&desc, hostHandle(handle));
}

void Device::mapTexture(HostHeapOffset handle, TextureBuffer *pTexture) {
    D3D12_SHADER_RESOURCE_VIEW_DESC desc = {
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM, // TODO: this doesnt account for pixel format
        .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Texture2D = { .MostDetailedMip = 0, .MipLevels = 1 },
    };

    get()->CreateShaderResourceView(pTexture->getResource(), &desc, hostHandle(handle));
}

static ID3D12Debug *getDeviceDebugInterface() {
    ID3D12Debug *pDebug = nullptr;

    if (HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&pDebug)); SUCCEEDED(hr)) {
        simcoe::logInfo("enabling d3d12 debug layer");
        pDebug->EnableDebugLayer();
    } else {
        simcoe::logWarn("failed to enable d3d12 debug layer");
    }

    return pDebug;
}

static ID3D12InfoQueue1 *getDeviceInfoQueue(DWORD *pCookie, ID3D12Device *pDevice) {
    ID3D12InfoQueue1 *pInfoQueue = nullptr;

    if (HRESULT hr = pDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue)); SUCCEEDED(hr)) {
        simcoe::logInfo("enabling d3d12 info queue");
        HR_CHECK(pInfoQueue->RegisterMessageCallback(debugCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, pCookie));
    } else {
        simcoe::logWarn("failed to enable d3d12 info queue");
    }

    return pInfoQueue;
}

static void setupDred() {
    ID3D12DeviceRemovedExtendedDataSettings *pSettings = nullptr;

    if (HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&pSettings)); SUCCEEDED(hr)) {
        simcoe::logInfo("enabling d3d12 device removed extended data");
        pSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        pSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        pSettings->Release();
    } else {
        simcoe::logWarn("failed to enable d3d12 device removed extended data");
    }
}

Device *Device::create(IDXGIAdapter4 *pAdapter, CreateFlags flags) {
    if (flags & eCreateExtendedInfo) {
        setupDred();
    }

    ID3D12Debug *pDebug = nullptr;
    if (flags & eCreateDebug) {
        pDebug = getDeviceDebugInterface();
    }

    ID3D12Device4 *pDevice = nullptr;
    HR_CHECK(D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDevice)));

    DWORD cookie = DWORD_MAX;
    ID3D12InfoQueue1 *pInfoQueue = nullptr;
    if (flags & eCreateInfoQueue) {
        pInfoQueue = getDeviceInfoQueue(&cookie, pDevice);
    }

    return new Device(pDevice, pDebug, pInfoQueue, cookie, flags, getRootSigVersion(pDevice));
}

Device::~Device() {
    if (pInfoQueue) {
        pInfoQueue->Release();
    }

    if (pDebug) {
        pDebug->Release();
    }
}

// adapter

Device *Adapter::createDevice(CreateFlags flags) {
    return Device::create(pAdapter, flags);
}

AdapterInfo Adapter::getInfo() {
    AdapterInfo info = {
        .name = util::narrow(desc.Description),
        .type = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) ? AdapterType::eSoftware : AdapterType::eDiscrete,
    };

    return info;
}

Adapter *Adapter::create(IDXGIAdapter1 *pAdapter) {
    IDXGIAdapter4 *pAdapter4 = nullptr;
    HR_CHECK(pAdapter->QueryInterface(IID_PPV_ARGS(&pAdapter4)));

    DXGI_ADAPTER_DESC1 desc;
    HR_CHECK(pAdapter4->GetDesc1(&desc));

    pAdapter->Release();

    return new Adapter(pAdapter4, desc);
}

Adapter::~Adapter() {
    pAdapter->Release();
}

// context

void Context::reportLiveObjects() {
    if (pDebug != nullptr) {
        logInfo("reporting dxgi live objects");
        pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
    } else {
        logWarn("cannot report dxgi live objects");
    }
}

std::vector<Adapter*> Context::getAdapters() {
    std::vector<Adapter*> adapters;

    IDXGIAdapter1 *pAdapter = nullptr;
    for (UINT i = 0; pFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        adapters.push_back(Adapter::create(pAdapter));
    }

    return adapters;
}

static IDXGIDebug1 *getDebugInterface() {
    IDXGIDebug1 *pDebug = nullptr;

    if (HRESULT hr = DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug)); SUCCEEDED(hr)) {
        simcoe::logInfo("enabling dxgi debug layer");
        pDebug->EnableLeakTrackingForThread();
    } else {
        simcoe::logWarn("failed to enable dxgi debug layer");
    }

    return pDebug;
}

Context *Context::create(CreateFlags flags) {
    UINT factoryFlags = 0;
    if (flags & eCreateDebug) {
        factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }

    IDXGIFactory6 *pFactory = nullptr;
    HR_CHECK(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&pFactory)));

    IDXGIDebug1 *pDebug = nullptr;
    if (flags & eCreateDebug) {
        pDebug = getDebugInterface();
    } else {
        simcoe::logInfo("dxgi debug layer not enabled");
    }

    return new Context(pFactory, pDebug);
}

Context::~Context() {
    pFactory->Release();
    reportLiveObjects();

    if (pDebug) { pDebug->Release(); }
}

// descriptor heap

DeviceHeapOffset DescriptorHeap::deviceOffset(UINT index) {
    return DeviceHeapOffset(get()->GetGPUDescriptorHandleForHeapStart().ptr + index * descriptorSize);
}

HostHeapOffset DescriptorHeap::hostOffset(UINT index) {
    return HostHeapOffset(get()->GetCPUDescriptorHandleForHeapStart().ptr + index * descriptorSize);
}

DescriptorHeap *DescriptorHeap::create(ID3D12DescriptorHeap *pHeap, UINT descriptorSize) {
    return new DescriptorHeap(pHeap, descriptorSize);
}

// pipeline state

void PipelineState::setName(std::string_view name) {
    ::setName(pRootSignature, std::format("{}.root", name));
    ::setName(pState, std::format("{}.state", name));
}

PipelineState *PipelineState::create(ID3D12RootSignature *pRootSignature, ID3D12PipelineState *pState, IndexMap textureInputs, IndexMap uniformInputs) {
    return new PipelineState(pRootSignature, pState, textureInputs, uniformInputs);
}

PipelineState::~PipelineState() {
    pRootSignature->Release();
    pState->Release();
}

// render target

RenderTarget *RenderTarget::create(ID3D12Resource *pResource) {
    return new RenderTarget(pResource);
}

// depth buffer

DepthBuffer *DepthBuffer::create(ID3D12Resource *pResource) {
    return new DepthBuffer(pResource);
}

// vertex buffer

VertexBuffer *VertexBuffer::create(ID3D12Resource *pResource, D3D12_VERTEX_BUFFER_VIEW view) {
    return new VertexBuffer(pResource, view);
}

// index buffer

IndexBuffer *IndexBuffer::create(ID3D12Resource *pResource, D3D12_INDEX_BUFFER_VIEW view) {
    return new IndexBuffer(pResource, view);
}

// texture buffer
TextureBuffer *TextureBuffer::create(ID3D12Resource *pResource) {
    return new TextureBuffer(pResource);
}

// uniform buffer

void UniformBuffer::write(const void *pData, size_t length) {
    memcpy(pMapped, pData, length);
}

UniformBuffer *UniformBuffer::create(ID3D12Resource *pResource, void *pMapped) {
    return new UniformBuffer(pResource, pMapped);
}

UniformBuffer::~UniformBuffer() {
    getResource()->Unmap(0, nullptr);
}

// upload buffer

UploadBuffer *UploadBuffer::create(ID3D12Resource *pResource) {
    return new UploadBuffer(pResource);
}

// fence

size_t Fence::getValue() {
    return get()->GetCompletedValue();
}

void Fence::wait(size_t value) {
    get()->SetEventOnCompletion(value, hEvent);
    WaitForSingleObject(hEvent, INFINITE);
}

Fence *Fence::create(ID3D12Fence *pFence, HANDLE hEvent) {
    return new Fence(pFence, hEvent);
}

Fence::~Fence() {
    CloseHandle(hEvent);
}
