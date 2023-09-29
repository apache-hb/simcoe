#include "engine/engine.h"

#include "engine/os/system.h"
#include "engine/render/render.h"

namespace render = simcoe::render;

struct RenderCreateInfo {
    HWND hWindow;

    UINT displayWidth;
    UINT displayHeight;

    UINT renderWidth;
    UINT renderHeight;
};

struct RenderContext {
    static constexpr UINT kBackBufferCount = 2;

    static RenderContext *create(const RenderCreateInfo& createInfo) {
        return new RenderContext(createInfo);
    }

    void render() {
        beginFrame();

        pQueue->execute(pCommands);
        pDisplayQueue->present();

        endFrame();
    }

private:
    RenderContext(const RenderCreateInfo& createInfo) {
        pContext = render::Context::create();
        adapters = pContext->getAdapters();
        auto adapter = selectAdapter();

        pDevice = adapter->createDevice();
        pQueue = pDevice->createQueue();

        const render::DisplayQueueCreateInfo displayCreateInfo = {
            .hWindow = createInfo.hWindow,
            .width = createInfo.displayWidth,
            .height = createInfo.displayHeight,
            .bufferCount = kBackBufferCount
        };

        pDisplayQueue = pQueue->createDisplayQueue(pContext, displayCreateInfo);
        pCommands = pDevice->createCommands();

        pRenderTargetHeap = pDevice->createRenderTargetHeap(kBackBufferCount);

        for (UINT i = 0; i < kBackBufferCount; ++i) {
            render::RenderTarget *pRenderTarget = pDisplayQueue->getRenderTarget(i);
            pDevice->mapRenderTarget(pRenderTargetHeap->hostOffset(i), pRenderTarget);

            pRenderTargetArray[i] = pRenderTarget;
        }

        pFence = pDevice->createFence();
    }

    void beginFrame() {
        frameIndex = pDisplayQueue->getFrameIndex();
        pCommands->begin();

        pCommands->transition(pRenderTargetArray[frameIndex], render::ResourceState::ePresent, render::ResourceState::eRenderTarget);
        pCommands->clearRenderTarget(pRenderTargetHeap->hostOffset(frameIndex), { 0.0f, 0.2f, 0.4f, 1.0f });
        pCommands->transition(pRenderTargetArray[frameIndex], render::ResourceState::eRenderTarget, render::ResourceState::ePresent);

        pCommands->end();
    }

    void endFrame() {
        size_t value = fenceValue++;
        pQueue->signal(pFence, value);

        if (pFence->getValue() < value) {
            pFence->wait(value);
        }
    }

    render::Adapter* selectAdapter() {
        return adapters[0];
    }

    render::Context *pContext;
    std::vector<render::Adapter*> adapters;
    render::Device *pDevice;

    render::DeviceQueue *pQueue;
    render::Commands *pCommands;

    render::DisplayQueue *pDisplayQueue;

    render::DescriptorHeap *pRenderTargetHeap;
    render::RenderTarget *pRenderTargetArray[kBackBufferCount];

    size_t frameIndex = 0;
    render::Fence *pFence;
    size_t fenceValue = 1;
};

static void commonMain(simcoe::System& system) {
    const simcoe::WindowCreateInfo windowCreateInfo = {
        .title = "simcoe",
        .style = simcoe::WindowStyle::eWindowed,
        .width = 800,
        .height = 600
    };

    simcoe::Window window = system.createWindow(windowCreateInfo);

    const RenderCreateInfo renderCreateInfo = {
        .hWindow = window.getHandle(),
        .displayWidth = 800,
        .displayHeight = 600,

        .renderWidth = 800,
        .renderHeight = 600
    };

    RenderContext *pContext = RenderContext::create(renderCreateInfo);

    while (system.getEvent()) {
        system.dispatchEvent();
        pContext->render();
    }
}

static int innerMain(simcoe::System& system) try {
    simcoe::logInfo("startup");
    commonMain(system);
    simcoe::logInfo("shutdown");

    return 0;
} catch (...) {
    simcoe::logError("unhandled exception");
    return 99;
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    simcoe::System system(hInstance, nCmdShow);
    return innerMain(system);
}

int main(int argc, const char **argv) {
    simcoe::System system(GetModuleHandle(nullptr), SW_SHOWDEFAULT);
    return innerMain(system);
}
