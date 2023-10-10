#include "engine/render/graph.h"

using namespace simcoe;
using namespace simcoe::render;

///
/// graph object
///

IGraphObject::IGraphObject(Graph *graph, std::string name, StateDep stateDeps)
    : graph(graph)
    , ctx(graph->ctx)
    , name(name)
    , stateDeps(StateDep(stateDeps | eDepDevice))
{ }

///
/// resource handle
///

rhi::ResourceState IResourceHandle::getCurrentState() const {
    return getResourceState(getResource());
}

void IResourceHandle::setCurrentState(rhi::ResourceState state) {
    setResourceState(getResource(), state);
}

rhi::ResourceState IResourceHandle::getResourceState(rhi::DeviceResource *pResource) const {
    ASSERTF(pResource != nullptr, "resource missing in {}", getName());
    return graph->getResourceState(getResource());
}

void IResourceHandle::setResourceState(rhi::DeviceResource *pResource, rhi::ResourceState state) {
    ASSERTF(pResource != nullptr, "resource missing in {}", getName());
    graph->setResourceState(pResource, state);
}

///
/// graph
///

void Graph::setFullscreen(bool bFullscreen) {
    changeData(StateDep::eNone, [=] {
        ctx->changeFullscreen(bFullscreen);
    });
}

void Graph::resizeDisplay(UINT width, UINT height) {
    const auto& createInfo = ctx->getCreateInfo();
    if (width == createInfo.displayWidth && height == createInfo.displayHeight)
        return;

    changeData(StateDep::eDepDisplaySize, [=] {
        ctx->changeDisplaySize(width, height);
    });
}

void Graph::resizeRender(UINT width, UINT height) {
    const auto& createInfo = ctx->getCreateInfo();
    if (width == createInfo.renderWidth && height == createInfo.renderHeight)
        return;

    changeData(StateDep::eDepRenderSize, [=] {
        ctx->changeRenderSize(width, height);
    });
}

void Graph::changeBackBufferCount(UINT count) {
    const auto& createInfo = ctx->getCreateInfo();
    if (count == createInfo.backBufferCount)
        return;

    changeData(StateDep::eDepBackBufferCount, [=] {
        ctx->changeBackBufferCount(count);
    });
}

void Graph::changeAdapter(UINT index) {
    const auto& createInfo = ctx->getCreateInfo();
    if (index == createInfo.adapterIndex)
        return;

    changeData(StateDep::eDepDevice, [=] {
        ctx->changeAdapter(index);
    });
}

void Graph::execute() {
    if (lock) { return; }

    std::lock_guard guard(renderLock);
    pCurrentRenderTarget = nullptr;

    ctx->beginRender();
    ctx->beginDirect();

    for (IRenderPass *pPass : passes) {
        executePass(pPass);
    }

    ctx->endDirect();
    ctx->endRender();
    ctx->waitForDirectQueue();
}

void Graph::createIf(StateDep dep) {
    for (IGraphObject *pObject : objects) {
        if (pObject->dependsOn(dep)) pObject->create();
    }

    for (IResourceHandle *pHandle : resources) {
        if (pHandle->dependsOn(dep)) pHandle->create();
    }

    for (IRenderPass *pPass : passes) {
        if (pPass->dependsOn(dep)) pPass->create();
    }
}

void Graph::destroyIf(StateDep dep) {
    for (IRenderPass *pPass : passes) {
        if (pPass->dependsOn(dep)) pPass->destroy();
    }

    for (IResourceHandle *pHandle : resources) {
        if (pHandle->dependsOn(dep)) pHandle->destroy();
    }

    for (IGraphObject *pObject : objects) {
        if (pObject->dependsOn(dep)) pObject->destroy();
    }
}

void Graph::executePass(IRenderPass *pPass) {
    for (const auto *pInput : pPass->inputs) {
        auto *pHandle = pInput->getResourceHandle();
        rhi::DeviceResource *pResource = pHandle->getResource();
        auto requiredState = pInput->getRequiredState();
        auto currentState = getResourceState(pResource);

        if (currentState != requiredState) {
            ctx->transition(pResource, currentState, requiredState);
            setResourceState(pResource, requiredState);
        }
    }

    pPass->execute();
}
