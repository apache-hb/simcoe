#include "editor/render/render.h"

#include <array>

using namespace editor;

namespace {
    const auto kScreenQuad = std::to_array<Vertex>({
        Vertex{ { 1.f, -1.f, 0.0f }, { 0.f, 0.f } }, // top left
        Vertex{ { 1.f, 1.f, 0.0f }, { 1.f, 0.f } }, // top right
        Vertex{ { -1.f, -1.f, 0.0f }, { 0.f, 1.f } }, // bottom left
        Vertex{ { -1.f, 1.f, 0.0f }, { 1.f, 1.f } } // bottom right
    });

    const auto kScreenQuadIndices = std::to_array<uint16_t>({
        0, 2, 1,
        1, 2, 3
    });
}

RenderContext *RenderContext::create(const RenderCreateInfo& createInfo) {
    return new RenderContext(createInfo);
}

RenderContext::~RenderContext() {
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

// create data that depends on the present queue
void RenderContext::createDisplayData() {
    // create swapchain
    const render::DisplayQueueCreateInfo displayCreateInfo = {
        .hWindow = createInfo.hWindow,
        .width = createInfo.displayWidth,
        .height = createInfo.displayHeight,
        .bufferCount = kBackBufferCount
    };

    pDisplayQueue = pDirectQueue->createDisplayQueue(pContext, displayCreateInfo);

    // create render targets
    frameIndex = pDisplayQueue->getFrameIndex();

    for (UINT i = 0; i < kBackBufferCount; ++i) {
        frameData[i] = { pDevice->createCommandMemory(render::CommandType::eDirect) };
    }

    pDirectCommands = pDevice->createCommands(render::CommandType::eDirect, frameData[frameIndex].pMemory);
}

void RenderContext::destroyDisplayData() {
    for (UINT i = 0; i < kBackBufferCount; ++i) {
        delete frameData[i].pMemory;
    }

    delete pDirectCommands;
    delete pDisplayQueue;
}

void RenderContext::createHeaps() {
    pRenderTargetAlloc = new RenderTargetAlloc(pDevice->createRenderTargetHeap(kBackBufferCount + 1));
    pDataAlloc = new DataAlloc(pDevice->createShaderDataHeap(3));
}

void RenderContext::destroyHeaps() {
    delete pDataAlloc;
    delete pRenderTargetAlloc;
}

void RenderContext::executePresent() {
    pDirectCommands->end();
    pDirectQueue->execute(pDirectCommands);
    pDisplayQueue->present();
}

void RenderContext::beginRender() {
    frameIndex = pDisplayQueue->getFrameIndex();
    pDirectCommands->begin(frameData[frameIndex].pMemory);
    pDirectCommands->setHeap(pDataAlloc->pHeap);
}

void RenderContext::endRender() {
    size_t value = frameData[frameIndex].fenceValue++;
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
    waitForCopy();
}

void RenderContext::waitForCopy() {
    size_t value = copyFenceValue++;
    pCopyQueue->signal(pFence, value);

    if (pFence->getValue() < value) {
        pFence->wait(value);
    }
}

