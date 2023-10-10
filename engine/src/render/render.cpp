#include "engine/render/render.h"

#include "engine/engine.h"

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
    pContext = rhi::Context::create();

    createContextData();
    createDeviceData();
    createHeaps();
    createDisplayData();
    createFrameData();
}

// create data that depends on the context
void Context::createContextData() {
    adapters = pContext->getAdapters();
}

void Context::destroyContextData() {
    for (auto pAdapter : adapters) {
        delete pAdapter;
    }
}

// create data that depends on the device
void Context::createDeviceData() {
    rhi::Adapter* pAdapter = selectAdapter();
    pDevice = pAdapter->createDevice();

    pDirectQueue = pDevice->createQueue(rhi::CommandType::eDirect);
    pCopyQueue = pDevice->createQueue(rhi::CommandType::eCopy);

    pCopyAllocator = pDevice->createCommandMemory(rhi::CommandType::eCopy);
    pCopyCommands = pDevice->createCommands(rhi::CommandType::eCopy, pCopyAllocator);

    pFence = pDevice->createFence();
}

void Context::destroyDeviceData() {
    delete pFence;

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
    delete pDisplayQueue;
}

// create data that relys on the number of backbuffers
void Context::createFrameData() {
    frameIndex = pDisplayQueue->getFrameIndex();
    bReportedFullscreen = pDisplayQueue->getFullscreenState();

    frameData.resize(createInfo.backBufferCount);
    for (UINT i = 0; i < createInfo.backBufferCount; i++) {
        frameData[i] = { pDevice->createCommandMemory(rhi::CommandType::eDirect) };
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

void Context::beginRender() {
    frameIndex = pDisplayQueue->getFrameIndex();
}

void Context::endRender() {
    pDisplayQueue->present(false);
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
    pDirectQueue->signal(pFence, value);

    if (pFence->getValue() < value) {
        pFence->wait(value);
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
    pCopyQueue->signal(pFence, value);

    if (pFence->getValue() <= value) {
        pFence->wait(value);
    }
}
