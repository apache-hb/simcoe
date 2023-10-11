#include "engine/render/render.h"

#include "engine/engine.h"

#include <DirectXMath.h>

using namespace simcoe;
using namespace simcoe::render;

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
    pContext = rhi::Context::create(rhi::eCreateDebug);

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
    rhi::Adapter* pAdapter = selectAdapter();
    rhi::CreateFlags deviceFlags = rhi::CreateFlags(rhi::eCreateDebug | rhi::eCreateInfoQueue | rhi::eCreateExtendedInfo);
    pDevice = pAdapter->createDevice(deviceFlags);

    pDirectQueue = pDevice->createQueue(rhi::CommandType::eDirect);
    pDirectFence = pDevice->createFence();

    pCopyQueue = pDevice->createQueue(rhi::CommandType::eCopy);
    pCopyFence = pDevice->createFence();

    pCopyAllocator = pDevice->createCommandMemory(rhi::CommandType::eCopy);
    pCopyCommands = pDevice->createCommands(rhi::CommandType::eCopy, pCopyAllocator);

    pDevice->setName("simcoe.device");
    pDirectQueue->setName("simcoe.direct-queue");
    pCopyQueue->setName("simcoe.copy-queue");
    pCopyAllocator->setName("simcoe.copy-allocator");
    pCopyCommands->setName("simcoe.copy-commands");
    pDirectFence->setName("simcoe.direct-fence");
    pCopyFence->setName("simcoe.copy-fence");
}

void Context::destroyDeviceData() {
    delete pDirectFence;
    delete pCopyFence;

    delete pCopyCommands;
    delete pCopyAllocator;

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

    pDataAlloc = new ShaderResourceAlloc(pDevice->createShaderDataHeap(64), 64);
}

void Context::destroyHeaps() {
    delete pDataAlloc;

    delete pDepthStencilAlloc;
    delete pRenderTargetAlloc;
}

void Context::changeFullscreen(bool bFullscreen) {
    destroyFrameData();
    pDisplayQueue->setFullscreenState(bFullscreen);
    createFrameData();
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
