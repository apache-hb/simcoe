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
    destroyResources();
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
    createResources();
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

void RenderContext::createResources() {
    // data to upload
    const auto quad = std::to_array<Vertex>({
        Vertex{ { 0.5f, -0.5f, 0.0f }, { 0.f, 0.f } }, // top left
        Vertex{ { 0.5f, 0.5f, 0.0f }, { 1.f, 0.f } }, // top right
        Vertex{ { -0.5f, -0.5f, 0.0f }, { 0.f, 1.f } }, // bottom left
        Vertex{ { -0.5f, 0.5f, 0.0f }, { 1.f, 1.f } } // bottom right
    });

    const auto indices = std::to_array<uint16_t>({
        0, 2, 1,
        1, 2, 3
    });

    assets::Image image = createInfo.depot.loadImage("uv-coords.png");
    const render::TextureInfo textureInfo = {
        .width = image.width,
        .height = image.height,

        .format = render::PixelFormat::eRGBA8
    };

    // create staging buffers

    std::unique_ptr<render::UploadBuffer> pVertexStaging{pDevice->createUploadBuffer(quad.data(), quad.size() * sizeof(Vertex))};
    std::unique_ptr<render::UploadBuffer> pIndexStaging{pDevice->createUploadBuffer(indices.data(), indices.size() * sizeof(uint16_t))};
    std::unique_ptr<render::UploadBuffer> pTextureStaging{pDevice->createTextureUploadBuffer(textureInfo)};

    // create destination buffers

    pQuadVertexBuffer = pDevice->createVertexBuffer(quad.size(), sizeof(Vertex));
    pQuadIndexBuffer = pDevice->createIndexBuffer(indices.size(), render::TypeFormat::eUint16);


    // upload data

    pDirectCommands->begin(frameData[frameIndex].pMemory);
    pCopyCommands->begin(pCopyAllocator);

    pCopyCommands->copyBuffer(pQuadVertexBuffer, pVertexStaging.get());
    pCopyCommands->copyBuffer(pQuadIndexBuffer, pIndexStaging.get());

    pCopyCommands->end();
    pDirectCommands->end();

    pCopyQueue->execute(pCopyCommands);
    waitForCopy();

    pDirectQueue->execute(pDirectCommands);
    endRender();
}

void RenderContext::destroyResources() {
    delete pQuadVertexBuffer;
    delete pQuadIndexBuffer;
}

// rendering

void RenderContext::executeScene(DataAlloc::Index quadUniformIndex, DataAlloc::Index quadTextureIndex, const RenderTarget& target) {
    auto renderTargetIndex = pRenderTargetAlloc->hostOffset(target.index);

    pDirectCommands->setRenderTarget(renderTargetIndex);
    pDirectCommands->clearRenderTarget(renderTargetIndex, kClearColour);

    // draw scene content

    // TODO: this requires some pretty fucked guessing, should sort that out
    pDirectCommands->setShaderInput(pDataAlloc->deviceOffset(quadTextureIndex), 0);
    pDirectCommands->setShaderInput(pDataAlloc->deviceOffset(quadUniformIndex), 1);

    pDirectCommands->setVertexBuffer(pQuadVertexBuffer);
    pDirectCommands->setIndexBuffer(pQuadIndexBuffer);
    pDirectCommands->drawIndexBuffer(6);
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

