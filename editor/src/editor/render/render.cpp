#include "editor/render/render.h"

#include "engine/engine.h"

using namespace editor;

RenderContext *RenderContext::create(const RenderCreateInfo& createInfo) {
    return new RenderContext(createInfo);
}

RenderContext::~RenderContext() {
    destroyFrameData();
    destroyDisplayData();
    destroyHeaps();
    destroyDeviceData();
    destroyContextData();

    delete pContext;
}

RenderContext::RenderContext(const RenderCreateInfo& createInfo) : createInfo(createInfo) {
    pContext = rhi::Context::create();

    createContextData();
    createDeviceData();
    createHeaps();
    createDisplayData();
    createFrameData();
}

// create data that depends on the context
void RenderContext::createContextData() {
    adapters = pContext->getAdapters();
}

void RenderContext::destroyContextData() {
    for (auto pAdapter : adapters) {
        delete pAdapter;
    }
}

// create data that depends on the device
void RenderContext::createDeviceData() {
    rhi::Adapter* pAdapter = selectAdapter();
    pDevice = pAdapter->createDevice();

    pDirectQueue = pDevice->createQueue(rhi::CommandType::eDirect);
    pCopyQueue = pDevice->createQueue(rhi::CommandType::eCopy);

    pCopyAllocator = pDevice->createCommandMemory(rhi::CommandType::eCopy);
    pCopyCommands = pDevice->createCommands(rhi::CommandType::eCopy, pCopyAllocator);

    pFence = pDevice->createFence();
}

void RenderContext::destroyDeviceData() {
    delete pFence;

    delete pCopyCommands;
    delete pCopyAllocator;

    delete pCopyQueue;
    delete pDirectQueue;
    delete pDevice;
}

// create data that depends on resolution
void RenderContext::createDisplayData() {
    // create swapchain
    const rhi::DisplayQueueCreateInfo displayCreateInfo = {
        .hWindow = createInfo.hWindow,
        .width = createInfo.displayWidth,
        .height = createInfo.displayHeight,
        .bufferCount = createInfo.backBufferCount,
    };

    pDisplayQueue = pDirectQueue->createDisplayQueue(pContext, displayCreateInfo);
}

void RenderContext::destroyDisplayData() {
    delete pDisplayQueue;
}

// create data that relys on the number of backbuffers
void RenderContext::createFrameData() {
    frameIndex = pDisplayQueue->getFrameIndex();
    fullscreen = pDisplayQueue->isFullscreen();

    frameData.resize(createInfo.backBufferCount);
    for (UINT i = 0; i < createInfo.backBufferCount; i++) {
        frameData[i] = { pDevice->createCommandMemory(rhi::CommandType::eDirect) };
    }

    pDirectCommands = pDevice->createCommands(rhi::CommandType::eDirect, frameData[frameIndex].pMemory);
}

void RenderContext::destroyFrameData() {
    for (UINT i = 0; i < createInfo.backBufferCount; ++i) {
        delete frameData[i].pMemory;
    }

    frameData.clear();

    delete pDirectCommands;
}

void RenderContext::createHeaps() {
    pRenderTargetAlloc = new RenderTargetAlloc(pDevice->createRenderTargetHeap(16), 16);
    pDepthStencilAlloc = new DepthStencilAlloc(pDevice->createDepthStencilHeap(16), 16);

    pDataAlloc = new ShaderResourceAlloc(pDevice->createShaderDataHeap(64), 64);
}

void RenderContext::destroyHeaps() {
    delete pDataAlloc;

    delete pDepthStencilAlloc;
    delete pRenderTargetAlloc;
}

void RenderContext::changeDisplaySize(UINT width, UINT height) {
    destroyFrameData();
    createInfo.displayWidth = width;
    createInfo.displayHeight = height;

    pDisplayQueue->resizeBuffers(createInfo.backBufferCount, width, height);
    createFrameData();
}

void RenderContext::changeRenderSize(UINT width, UINT height) {
    createInfo.renderWidth = width;
    createInfo.renderHeight = height;
}

void RenderContext::changeBackBufferCount(UINT count) {
    destroyFrameData();

    createInfo.backBufferCount = count;

    pDisplayQueue->resizeBuffers(count, createInfo.displayWidth, createInfo.displayHeight);
    createFrameData();
}

void RenderContext::changeAdapter(size_t index) {
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

void RenderContext::beginRender() {
    frameIndex = pDisplayQueue->getFrameIndex();
}

void RenderContext::endRender() {
    pDisplayQueue->present(!fullscreen);
}

void RenderContext::beginDirect() {
    pDirectCommands->begin(frameData[frameIndex].pMemory);
    pDirectCommands->setHeap(pDataAlloc->pHeap);
}

void RenderContext::endDirect() {
    pDirectCommands->end();
    pDirectQueue->execute(pDirectCommands);
}

void RenderContext::waitForDirectQueue() {
    size_t value = directFenceValue++;
    pDirectQueue->signal(pFence, value);

    if (pFence->getValue() < value) {
        pFence->wait(value);
    }
}

void RenderContext::beginCopy() {
    pCopyCommands->begin(pCopyAllocator);
}

void RenderContext::endCopy() {
    pCopyCommands->end();
    pCopyQueue->execute(pCopyCommands);
    waitForCopyQueue();
}

void RenderContext::waitForCopyQueue() {
    size_t value = copyFenceValue++;
    pCopyQueue->signal(pFence, value);

    if (pFence->getValue() <= value) {
        pFence->wait(value);
    }
}
