#include "engine/render/service.h"

#include "engine/config/system.h"

#include "engine/render/render.h"
#include "engine/render/graph.h"

#include "engine/threads/queue.h"

using namespace simcoe;

config::ConfigValue<size_t> cfgRenderFaults("render", "fault_limit", "Total render faults to tolerate before giving up rendering", 1);

config::ConfigValue<size_t> cfgRenderWidth("render", "draw_width", "Render width", 1920);
config::ConfigValue<size_t> cfgRenderHeight("render", "draw_height", "Render height", 1080);

config::ConfigValue<size_t> cfgBackBufferCount("render", "backBufferCount", "How many backbuffers to use", 2);

config::ConfigValue<size_t> cfgRenderQueueSize("render/worker", "queue_size", "upper limit on pending render events before blocking", 64);

namespace {
    render::Context *pContext = nullptr;
    render::Graph *pGraph = nullptr;

    size_t gRenderFaults = 0;

    threads::WorkQueue *pRenderQueue = nullptr;
    threads::ThreadHandle *pRenderThread = nullptr;
}

bool RenderService::createService() {
    auto& window = PlatformService::getWindow();
    auto size = window.getSize().as<uint32_t>();
    const render::RenderCreateInfo createInfo = {
        .hWindow = window.getHandle(),

        .backBufferCount = core::intCast<UINT>(cfgBackBufferCount.getCurrentValue()),

        .displayWidth = size.width,
        .displayHeight = size.height,

        .renderWidth = core::intCast<UINT>(cfgRenderWidth.getCurrentValue()),
        .renderHeight = core::intCast<UINT>(cfgRenderHeight.getCurrentValue())
    };

    pContext = render::Context::create(createInfo);
    pGraph = new render::Graph(pContext);

    pRenderQueue = new threads::WorkQueue(cfgRenderQueueSize.getCurrentValue());

    return true;
}

void RenderService::destroyService() {
    delete pRenderQueue;

    delete pGraph;
    delete pContext;
}

void RenderService::start() {
    pRenderThread = ThreadService::newThread(threads::eRealtime, "render", [&](auto token) {
        while (!token.stop_requested()) {
            draw();
        }
    });
}

void RenderService::shutdown() {
    pRenderThread->join();
}

void RenderService::enqueueWork(std::string name, threads::WorkItem&& work) {
    pRenderQueue->add(std::move(name), std::move(work));
} 

render::Graph *RenderService::getGraph() {
    return pGraph;
}

void RenderService::draw() {
    pRenderQueue->tryGetMessage();

    try {
        pGraph->execute();
    } catch (const core::Error& e) {
        if (!e.recoverable()) {
            throw;
        }

        gRenderFaults += 1;
        LOG_ERROR("fault: {}", e.what());
        LOG_ERROR("render fault. {} total fault{}", gRenderFaults, gRenderFaults > 1 ? "s" : "");

        auto faultLimit = cfgRenderFaults.getCurrentValue();
        if (gRenderFaults >= faultLimit) {
            LOG_ERROR("render fault exceeded limit of {}. exiting...", faultLimit);
            throw;
        }

        LOG_ERROR("attempting to recover...");
        pGraph->resumeFromFault();
    }
}

void RenderService::changeBackBufferCount(UINT newCount) {
    pRenderQueue->add("backbufferchange", [newCount]() {
        pGraph->changeBackBufferCount(newCount);
        LOG_INFO("changed backbuffer count to: {}", newCount);
    });
}

void RenderService::changeAdapter(UINT newAdapter) {
    pRenderQueue->add("adapterchange", [newAdapter]() {
        pGraph->changeAdapter(newAdapter);
        LOG_INFO("changed adapter to: {}", newAdapter);
    });
}

void RenderService::resizeDisplay(const WindowSize& event) {
    pRenderQueue->add("displayresize", [event]() {
        pGraph->resizeDisplay(event.width, event.height);
        LOG_INFO("changed display size to: {}x{}", event.width, event.height);
    });
}

void RenderService::resizeRender(const math::uint2& event) {
    pRenderQueue->add("renderresize", [event]() {
        pGraph->resizeRender(event.width, event.height);
        LOG_INFO("changed render size to: {}x{}", event.x, event.y);
    });
}

void RenderService::setFullscreen(bool bFullscreen) {
    pGraph->setFullscreen(bFullscreen);
}
