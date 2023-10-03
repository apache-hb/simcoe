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
    destroyPostData();
    destroyDisplayData();
    destroyDeviceData();
    destroyContextData();

    delete pContext;
}

RenderContext::RenderContext(const RenderCreateInfo& createInfo) : createInfo(createInfo) {
    pContext = render::Context::create();
    
    createContextData();
    createDeviceData(selectAdapter());
    createDisplayData();
    createPostData();
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
    render::DescriptorHeap *pHeap = pDevice->createRenderTargetHeap(kBackBufferCount + 1);
    pRenderTargetAlloc = new RenderTargetAlloc(pHeap);

    frameIndex = pDisplayQueue->getFrameIndex();

    for (UINT i = 0; i < kBackBufferCount; ++i) {
        render::CommandMemory *pMemory = pDevice->createCommandMemory(render::CommandType::eDirect);
        render::RenderTarget *pRenderTarget = pDisplayQueue->getRenderTarget(i);
        RenderTargetAlloc::Index rtvIndex = pRenderTargetAlloc->alloc();

        pDevice->mapRenderTarget(pRenderTargetAlloc->hostOffset(rtvIndex), pRenderTarget);

        frameData[i] = { pRenderTarget, rtvIndex, pMemory };
    }

    pDirectCommands = pDevice->createCommands(render::CommandType::eDirect, frameData[frameIndex].pMemory);
}

void RenderContext::destroyDisplayData() {
    for (UINT i = 0; i < kBackBufferCount; ++i) {
        delete frameData[i].pMemory;
        delete frameData[i].pRenderTarget;
    }

    delete pRenderTargetAlloc;
    delete pDirectCommands;
    delete pDisplayQueue;
}

void RenderContext::createPostData() {
    // create texture heap

    render::DescriptorHeap *pHeap = pDevice->createShaderDataHeap(3);
    pDataAlloc = new DataAlloc(pHeap);

    // upload data

    std::unique_ptr<render::UploadBuffer> pVertexStaging{pDevice->createUploadBuffer(kScreenQuad.data(), kScreenQuad.size() * sizeof(Vertex))};
    std::unique_ptr<render::UploadBuffer> pIndexStaging{pDevice->createUploadBuffer(kScreenQuadIndices.data(), kScreenQuadIndices.size() * sizeof(uint16_t))};

    pScreenQuadVerts = pDevice->createVertexBuffer(kScreenQuad.size(), sizeof(Vertex));
    pScreenQuadIndices = pDevice->createIndexBuffer(kScreenQuadIndices.size(), render::TypeFormat::eUint16);

    pCopyCommands->begin(pCopyAllocator);

    pCopyCommands->copyBuffer(pScreenQuadVerts, pVertexStaging.get());
    pCopyCommands->copyBuffer(pScreenQuadIndices, pIndexStaging.get());

    pCopyCommands->end();

    pCopyQueue->execute(pCopyCommands);
    waitForCopy();
}

void RenderContext::destroyPostData() {
    delete pDataAlloc;
    delete pScreenQuadVerts;
    delete pScreenQuadIndices;
}

void RenderContext::executePost(DataAlloc::Index sceneTarget) {
    auto renderTargetHeapIndex = frameData[frameIndex].renderTargetHeapIndex;
    render::RenderTarget *pRenderTarget = frameData[frameIndex].pRenderTarget;

    pDirectCommands->transition(pRenderTarget, render::ResourceState::ePresent, render::ResourceState::eRenderTarget);

    // set the actual back buffer as the render target
    pDirectCommands->setRenderTarget(pRenderTargetAlloc->hostOffset(renderTargetHeapIndex));
    pDirectCommands->clearRenderTarget(pRenderTargetAlloc->hostOffset(renderTargetHeapIndex), kBlackClearColour);

    // blit scene to backbuffer
    pDirectCommands->setShaderInput(pDataAlloc->deviceOffset(sceneTarget), 0);
    pDirectCommands->setVertexBuffer(pScreenQuadVerts);
    pDirectCommands->setIndexBuffer(pScreenQuadIndices);
    pDirectCommands->drawIndexBuffer(6);

    pDirectCommands->transition(pRenderTarget, render::ResourceState::eRenderTarget, render::ResourceState::ePresent);
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

