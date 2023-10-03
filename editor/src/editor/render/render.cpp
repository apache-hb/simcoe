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
    pContext = render::Context::create();

    createContextData();
    createDeviceData(selectAdapter());
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
void RenderContext::createDeviceData(render::Adapter* pAdapter) {
    pDevice = pAdapter->createDevice();

    pDirectQueue = pDevice->createQueue(render::CommandType::eDirect);
    pCopyQueue = pDevice->createQueue(render::CommandType::eCopy);

    pCopyAllocator = pDevice->createCommandMemory(render::CommandType::eCopy);
    pCopyCommands = pDevice->createCommands(render::CommandType::eCopy, pCopyAllocator);

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
    const render::DisplayQueueCreateInfo displayCreateInfo = {
        .hWindow = createInfo.hWindow,
        .width = createInfo.displayWidth,
        .height = createInfo.displayHeight,
        .bufferCount = kBackBufferCount
    };

    pDisplayQueue = pDirectQueue->createDisplayQueue(pContext, displayCreateInfo);
}

void RenderContext::destroyDisplayData() {
    delete pDisplayQueue;
}

// create data that relys on the number of backbuffers
void RenderContext::createFrameData() {
    frameIndex = pDisplayQueue->getFrameIndex();

    for (UINT i = 0; i < kBackBufferCount; ++i) {
        frameData[i] = { pDevice->createCommandMemory(render::CommandType::eDirect) };
    }

    pDirectCommands = pDevice->createCommands(render::CommandType::eDirect, frameData[frameIndex].pMemory);
}

void RenderContext::destroyFrameData() {
    for (UINT i = 0; i < kBackBufferCount; ++i) {
        delete frameData[i].pMemory;
    }

    delete pDirectCommands;
}

void RenderContext::createHeaps() {
    pRenderTargetAlloc = new RenderTargetAlloc(pDevice->createRenderTargetHeap(kBackBufferCount + 1));
    pDataAlloc = new DataAlloc(pDevice->createShaderDataHeap(1024));
}

void RenderContext::destroyHeaps() {
    delete pDataAlloc;
    delete pRenderTargetAlloc;
}

void RenderContext::changeDisplaySize(UINT width, UINT height) {
    pRenderTargetAlloc->offset = 0;
    pDataAlloc->offset = 0;

    destroyDisplayData();
    createInfo.displayWidth = width;
    createInfo.displayHeight = height;
    createDisplayData();
}

void RenderContext::flush() {
    waitForDirectQueue();

    directFenceValue = 1;
}

void RenderContext::beginRender() {
    frameIndex = pDisplayQueue->getFrameIndex();
}

void RenderContext::endRender() {
    pDisplayQueue->present();
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
