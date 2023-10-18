#include "engine/render/render.h"

#include "engine/engine.h"

#include <DirectXMath.h>

using namespace simcoe;
using namespace simcoe::render;

namespace {
#if DEBUG_RENDER
    constexpr auto kFactoryFlags = rhi::eCreateDebug;
    constexpr auto kDeviceFlags = rhi::CreateFlags(rhi::eCreateDebug | rhi::eCreateInfoQueue | rhi::eCreateExtendedInfo);
#else
    constexpr auto kFactoryFlags = rhi::eCreateNone;
    constexpr auto kDeviceFlags = rhi::eCreateInfoQueue;
#endif
}

Context *Context::create(const RenderCreateInfo& createInfo) {
    return new Context(createInfo);
}

Context::~Context() {
    destroyFrameData();
    destroyDisplayData();
    destroyHeaps();
    destroyDeviceData();
    destroyContextData();

    delete pContext;
}

Context::Context(const RenderCreateInfo& createInfo) : createInfo(createInfo) {
    pContext = rhi::Context::create(kFactoryFlags);

    createContextData();
    createDeviceData();
    createHeaps();
    createDisplayData();
    createFrameData();
}

// create data that depends on the context
void Context::createContextData() {
    adapters = pContext->getAdapters();
    simcoe::logInfo("found {} adapters, selecting adapter #{}", adapters.size(), createInfo.adapterIndex + 1);
}

void Context::destroyContextData() {
    for (auto pAdapter : adapters) {
        delete pAdapter;
    }
}

// create data that depends on the device
void Context::createDeviceData() {
    // create device
    rhi::Adapter* pAdapter = selectAdapter();

    pDevice = pAdapter->createDevice(kDeviceFlags);
    pDevice->setName("simcoe.device");

    // create direct queue and fence

    pDirectQueue = pDevice->createQueue(rhi::CommandType::eDirect);
    pDirectFence = pDevice->createFence();
    pDirectQueue->setName("simcoe.direct-queue");
    pDirectFence->setName("simcoe.direct-fence");

    // create copy queue and fence

    pCopyQueue = pDevice->createQueue(rhi::CommandType::eCopy);
    pCopyFence = pDevice->createFence();
    pCopyQueue->setName("simcoe.copy-queue");
    pCopyFence->setName("simcoe.copy-fence");

    // create copy commands and allocator

    pCopyAllocator = pDevice->createCommandMemory(rhi::CommandType::eCopy);
    pCopyCommands = pDevice->createCommands(rhi::CommandType::eCopy, pCopyAllocator);
    pCopyAllocator->setName("simcoe.copy-allocator");
    pCopyCommands->setName("simcoe.copy-commands");

    // create compute queue and fence

    pComputeQueue = pDevice->createQueue(rhi::CommandType::eCompute);
    pComputeFence = pDevice->createFence();
    pComputeQueue->setName("simcoe.compute-queue");
    pComputeFence->setName("simcoe.compute-fence");

    // create compute commands and allocator

    pComputeAllocator = pDevice->createCommandMemory(rhi::CommandType::eCompute);
    pComputeCommands = pDevice->createCommands(rhi::CommandType::eCompute, pComputeAllocator);
    pComputeAllocator->setName("simcoe.compute-allocator");
    pComputeCommands->setName("simcoe.compute-commands");
}

void Context::destroyDeviceData() {
    delete pDirectFence;
    delete pCopyFence;
    delete pComputeFence;

    delete pCopyCommands;
    delete pCopyAllocator;

    delete pComputeCommands;
    delete pComputeAllocator;

    delete pComputeQueue;
    delete pCopyQueue;
    delete pDirectQueue;

    delete pDevice;
}

// create data that depends on resolution
void Context::createDisplayData() {
    // create swapchain
    const rhi::DisplayQueueCreateInfo displayCreateInfo = {
        .hWindow = createInfo.hWindow,
        .width = createInfo.displayWidth,
        .height = createInfo.displayHeight,
        .bufferCount = createInfo.backBufferCount,
        .format = getSwapChainFormat()
    };

    pDisplayQueue = pDirectQueue->createDisplayQueue(pContext, displayCreateInfo);
}

void Context::destroyDisplayData() {
    // the display queue cannot be destroyed in fullscreen mode
    if (bReportedFullscreen) {
        pDisplayQueue->setFullscreenState(false);
    }

    delete pDisplayQueue;
}

// create data that relys on the number of backbuffers
void Context::createFrameData() {
    frameIndex = pDisplayQueue->getFrameIndex();
    bReportedFullscreen = pDisplayQueue->getFullscreenState();

    frameData.resize(createInfo.backBufferCount);
    for (UINT i = 0; i < createInfo.backBufferCount; i++) {
        rhi::CommandMemory *pMemory = pDevice->createCommandMemory(rhi::CommandType::eDirect);
        pMemory->setName("simcoe.frame-" + std::to_string(i));

        frameData[i] = { pMemory };
    }

    pDirectCommands = pDevice->createCommands(rhi::CommandType::eDirect, frameData[frameIndex].pMemory);
}

void Context::destroyFrameData() {
    for (UINT i = 0; i < createInfo.backBufferCount; ++i) {
        delete frameData[i].pMemory;
    }

    frameData.clear();

    delete pDirectCommands;
}

void Context::createHeaps() {
    pRenderTargetAlloc = new RenderTargetAlloc(pDevice->createRenderTargetHeap(16), 16);
    pDepthStencilAlloc = new DepthStencilAlloc(pDevice->createDepthStencilHeap(16), 16);

    pDataAlloc = new ShaderResourceAlloc(pDevice->createShaderDataHeap(createInfo.srvHeapSize), createInfo.srvHeapSize);
}

void Context::destroyHeaps() {
    delete pDataAlloc;

    delete pDepthStencilAlloc;
    delete pRenderTargetAlloc;
}

void Context::changeFullscreen(bool bFullscreen) {
    pDisplayQueue->setFullscreenState(bFullscreen);
}

void Context::changeDisplaySize(UINT width, UINT height) {
    destroyFrameData();

    createInfo.displayWidth = width;
    createInfo.displayHeight = height;
    pDisplayQueue->resizeBuffers(createInfo.backBufferCount, width, height);

    createFrameData();
}

void Context::changeRenderSize(UINT width, UINT height) {
    createInfo.renderWidth = width;
    createInfo.renderHeight = height;
}

void Context::changeBackBufferCount(UINT count) {
    destroyFrameData();

    createInfo.backBufferCount = count;
    pDisplayQueue->resizeBuffers(count, createInfo.displayWidth, createInfo.displayHeight);

    createFrameData();
}

void Context::changeAdapter(size_t index) {
    destroyFrameData();
    destroyHeaps();
    destroyDisplayData();
    destroyDeviceData();

    createInfo.adapterIndex = index;

    createDeviceData();
    createDisplayData();
    createHeaps();
    createFrameData();
}

void Context::resumeFromFault() {
    destroyFrameData();
    destroyHeaps();
    destroyDisplayData();
    destroyDeviceData();
    destroyContextData();

    // device may have been removed, get a new list of devices
    pContext->reportLiveObjects();

    createContextData();
    createDeviceData();
    createDisplayData();
    createHeaps();
    createFrameData();
}

void Context::reportFaultInfo() {
    pDevice->reportFaultInfo();
}

void Context::beginRender() {
    frameIndex = pDisplayQueue->getFrameIndex();
}

void Context::endRender() {
    pDisplayQueue->present(bReportedFullscreen ? false : bAllowTearing.load());
}

void Context::beginDirect() {
    pDirectCommands->begin(frameData[frameIndex].pMemory);
    pDirectCommands->setHeap(pDataAlloc->pHeap);
}

void Context::endDirect() {
    pDirectCommands->end();
    pDirectQueue->execute(pDirectCommands);
}

void Context::waitForDirectQueue() {
    size_t value = directFenceValue++;
    pDirectQueue->signal(pDirectFence, value);

    if (pDirectFence->getValue() <= value) {
        pDirectFence->wait(value);
    }
}

void Context::beginCopy() {
    pCopyCommands->begin(pCopyAllocator);
}

void Context::endCopy() {
    pCopyCommands->end();
    pCopyQueue->execute(pCopyCommands);
    waitForCopyQueue();
}

void Context::waitForCopyQueue() {
    size_t value = copyFenceValue++;
    pCopyQueue->signal(pCopyFence, value);

    if (pCopyFence->getValue() <= value) {
        pCopyFence->wait(value);
    }
}

void Context::beginCompute() {
    pComputeCommands->begin(pComputeAllocator);
}

void Context::endCompute() {
    pComputeCommands->end();
    pComputeQueue->execute(pComputeCommands);
    waitForComputeQueue();
}

void Context::waitForComputeQueue() {
    size_t value = computeFenceValue++;
    pComputeQueue->signal(pComputeFence, value);

    if (pComputeFence->getValue() <= value) {
        pComputeFence->wait(value);
    }
}
