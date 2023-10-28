#include "engine/render/graph.h"

#include "engine/core/panic.h"

using namespace simcoe;
using namespace simcoe::render;

///
/// graph object
///

IGraphObject::IGraphObject(Graph *pGraph, std::string name, StateDep stateDeps)
    : pGraph(pGraph)
    , ctx(pGraph->getContext())
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
    return pGraph->getResourceState(getResource());
}

void IResourceHandle::setResourceState(rhi::DeviceResource *pResource, rhi::ResourceState state) {
    ASSERTF(pResource != nullptr, "resource missing in {}", getName());
    pGraph->setResourceState(pResource, state);
}

///
/// render pass
///

void IRenderPass::executePass() {
    ASSERTF(pRenderTarget != nullptr, "no render target set for pass {}", getName());
    IRTVHandle *pNewTarget = pRenderTarget->getInner();
    auto rtvIndex = pNewTarget->getRtvIndex();
    auto newClear = pNewTarget->getClearColour();

    IRTVHandle *pCurrentTarget = pGraph->pCurrentRenderTarget;

    if (pDepthStencil != nullptr) {
        IDSVHandle *pDepth = pDepthStencil->getInner();
        auto dsvIndex = pDepth->getDsvIndex();

        ctx->setRenderAndDepth(rtvIndex, dsvIndex);
        ctx->clearDepthStencil(dsvIndex, 1.f, 0);

        if (pNewTarget != pCurrentTarget) {
            ctx->clearRenderTarget(rtvIndex, newClear);
            pGraph->pCurrentRenderTarget = pNewTarget;
        }

    } else if (pNewTarget != pCurrentTarget) {
        ctx->setRenderTarget(rtvIndex);
        ctx->clearRenderTarget(rtvIndex, newClear);
        pGraph->pCurrentRenderTarget = pNewTarget;
    }

    execute();
}

///
/// graph
///

void Graph::removePass(ICommandPass *pPass) {
    std::lock_guard guard(renderLock);

    std::erase(passes, pPass);
    pPass->destroy();
    delete pPass;
}

void Graph::removeResource(IResourceHandle *pHandle) {
    std::lock_guard guard(renderLock);

    std::erase(resources, pHandle);
    pHandle->destroy();
    delete pHandle;
}

void Graph::removeObject(IGraphObject *pObject) {
    std::lock_guard guard(renderLock);

    std::erase(objects, pObject);
    pObject->destroy();
    delete pObject;
}

void Graph::setFullscreen(bool bFullscreen) {
    if (ctx->bReportedFullscreen == bFullscreen)
        return;

    changeData(StateDep::eDepDisplaySize, [=] {
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

void Graph::resumeFromFault() {
    LOG_INFO("resuming from fault");
    ctx->reportFaultInfo();

    changeData(StateDep::eDepDevice, [=] {
        ctx->resumeFromFault();
    });
}

bool Graph::execute() {
    if (lock) { return false; }

    std::lock_guard guard(renderLock);
    pCurrentRenderTarget = nullptr;

    ctx->beginRender();
    ctx->beginDirect();

    for (ICommandPass *pPass : passes) {
        executePass(pPass);
    }

    ctx->endDirect();
    ctx->endRender();
    ctx->waitForDirectQueue();

    return true;
}

void Graph::createIf(StateDep dep) {
    for (IGraphObject *pObject : objects) {
        if (pObject->dependsOn(dep)) pObject->create();
    }

    for (IResourceHandle *pHandle : resources) {
        if (pHandle->dependsOn(dep)) pHandle->create();
    }

    for (ICommandPass *pPass : passes) {
        if (pPass->dependsOn(dep)) pPass->create();
    }
}

void Graph::destroyIf(StateDep dep) {
    for (ICommandPass *pPass : passes) {
        if (pPass->dependsOn(dep)) pPass->destroy();
    }

    for (IResourceHandle *pHandle : resources) {
        if (pHandle->dependsOn(dep)) pHandle->destroy();
    }

    for (IGraphObject *pObject : objects) {
        if (pObject->dependsOn(dep)) pObject->destroy();
    }
}

void Graph::executePass(ICommandPass *pPass) {
    std::vector<rhi::Transition> barriers;
    for (const auto *pInput : pPass->inputs) {
        auto *pHandle = pInput->getResourceHandle();
        rhi::DeviceResource *pResource = pHandle->getResource();
        auto requiredState = pInput->getRequiredState();
        auto currentState = getResourceState(pResource);

        if (currentState != requiredState) {
            barriers.push_back({ pResource, currentState, requiredState });
            setResourceState(pResource, requiredState);
        }
    }

    if (!barriers.empty()) {
        ctx->transition(barriers);
    }

    pPass->executePass();
}

///
/// state managment
///

void Graph::addResourceObject(IResourceHandle *pHandle) {
    withLock([=] {
        pHandle->create();
        resources.push_back(pHandle);
    });
}

void Graph::addPassObject(ICommandPass *pPass) {
    withLock([=] {
        pPass->create();
        passes.push_back(pPass);
    });
}

void Graph::addGraphObject(IGraphObject *pObject) {
    withLock([=] {
        pObject->create();
        objects.push_back(pObject);
    });
}
