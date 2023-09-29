#include "engine/engine.h"
#include "engine/os/system.h"
#include "engine/assets/assets.h"
#include "engine/render/render.h"

#include <array>

using namespace simcoe;

struct RenderCreateInfo {
    HWND hWindow;
    assets::Assets& depot;

    UINT displayWidth;
    UINT displayHeight;

    UINT renderWidth;
    UINT renderHeight;
};

struct Vertex {
    math::float3 position;
    math::float4 colour;
};

struct RenderContext {
    static constexpr UINT kBackBufferCount = 2;

    static RenderContext *create(const RenderCreateInfo& createInfo) {
        return new RenderContext(createInfo);
    }

    ~RenderContext() {
        destroyResources();
        destroyDisplayData();
        destroyDeviceData();
        destroyContextData();

        delete pContext;
    }

    void render() {
        beginFrame();

        pQueue->execute(pCommands);
        pDisplayQueue->present();

        endFrame();
    }

private:
    RenderContext(const RenderCreateInfo& createInfo) : createInfo(createInfo) {
        pContext = render::Context::create();
        createContextData();
        createDeviceData(selectAdapter());
        createDisplayData();
        createResources();
    }

    // state selection

    render::Adapter* selectAdapter() {
        return adapters[0];
    }

    // create data that depends on the context
    void createContextData() {
        adapters = pContext->getAdapters();
    }

    void destroyContextData() {
        for (auto pAdapter : adapters) {
            delete pAdapter;
        }
    }

    // create data that depends on the device
    void createDeviceData(render::Adapter* pAdapter) {
        pDevice = pAdapter->createDevice();
        pQueue = pDevice->createQueue();
        pFence = pDevice->createFence();
    }

    void destroyDeviceData() {
        delete pFence;
        delete pCommands;
        delete pQueue;
        delete pDevice;
    }

    // create data that depends on the present queue
    void createDisplayData() {
        viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(createInfo.renderWidth),
            .height = static_cast<float>(createInfo.renderHeight),

            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        scissor = {
            .left = 0,
            .top = 0,
            .right = static_cast<LONG>(createInfo.renderWidth),
            .bottom = static_cast<LONG>(createInfo.renderHeight)
        };

        const render::DisplayQueueCreateInfo displayCreateInfo = {
            .hWindow = createInfo.hWindow,
            .width = createInfo.displayWidth,
            .height = createInfo.displayHeight,
            .bufferCount = kBackBufferCount
        };

        pDisplayQueue = pQueue->createDisplayQueue(pContext, displayCreateInfo);
        pRenderTargetHeap = pDevice->createRenderTargetHeap(kBackBufferCount);

        for (UINT i = 0; i < kBackBufferCount; ++i) {
            render::RenderTarget *pRenderTarget = pDisplayQueue->getRenderTarget(i);
            pDevice->mapRenderTarget(pRenderTargetHeap->hostOffset(i), pRenderTarget);

            pRenderTargetArray[i] = pRenderTarget;

            pMemoryArray[i] = pDevice->createCommandMemory();
        }

        pCommands = pDevice->createCommands(pMemoryArray[0]);
    }

    void createResources() {
        const auto quad = std::to_array<Vertex>({
            Vertex{ { 0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } }, // top left
            Vertex{ { 0.5f, 0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } }, // top right
            Vertex{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }, // bottom left
            Vertex{ { -0.5f, 0.5f, 0.0f }, { 1.0f, 1.0f, 0.0f, 1.0f } } // bottom right
        });

        const auto indices = std::to_array<uint16_t>({
            0, 2, 1,
            1, 2, 3
        });

        pVertexBuffer = pDevice->createVertexBuffer<Vertex>(quad);
        pIndexBuffer = pDevice->createIndexBuffer<uint16_t>(indices, render::TypeFormat::eUint16);

        const render::PipelineCreateInfo psoCreateInfo = {
            .vertexShader = createInfo.depot.load("triangle.vs.cso"),
            .pixelShader = createInfo.depot.load("triangle.ps.cso"),

            .attributes = {
                { "POSITION", offsetof(Vertex, position), render::TypeFormat::eFloat3 },
                { "COLOUR", offsetof(Vertex, colour), render::TypeFormat::eFloat4 }
            }
        };

        pPipeline = pDevice->createPipelineState(psoCreateInfo);
    }

    void destroyResources() {
        delete pPipeline;
        delete pVertexBuffer;
        delete pIndexBuffer;
    }

    void destroyDisplayData() {
        for (UINT i = 0; i < kBackBufferCount; ++i) {
            delete pRenderTargetArray[i];
        }

        delete pRenderTargetHeap;
        delete pDisplayQueue;
    }

    // rendering

    void beginFrame() {
        frameIndex = pDisplayQueue->getFrameIndex();
        render::HostHeapOffset renderTarget = pRenderTargetHeap->hostOffset(frameIndex);
        pCommands->begin(pMemoryArray[frameIndex]);

        pCommands->setPipelineState(pPipeline);
        pCommands->setDisplay({ viewport, scissor });

        pCommands->transition(pRenderTargetArray[frameIndex], render::ResourceState::ePresent, render::ResourceState::eRenderTarget);

        pCommands->setRenderTarget(renderTarget);
        pCommands->clearRenderTarget(renderTarget, { 0.0f, 0.2f, 0.4f, 1.0f });

        pCommands->setVertexBuffer(pVertexBuffer);
        pCommands->setIndexBuffer(pIndexBuffer);
        pCommands->drawIndexBuffer(6);

        pCommands->transition(pRenderTargetArray[frameIndex], render::ResourceState::eRenderTarget, render::ResourceState::ePresent);

        pCommands->end();
    }

    void endFrame() {
        size_t value = fenceValues[frameIndex]++;
        pQueue->signal(pFence, value);

        if (pFence->getValue() < value) {
            pFence->wait(value);
        }
    }

    RenderCreateInfo createInfo;

    // device data

    render::Context *pContext;
    std::vector<render::Adapter*> adapters;
    render::Device *pDevice;

    render::DeviceQueue *pQueue;

    render::CommandMemory *pMemoryArray[kBackBufferCount];
    render::Commands *pCommands;

    size_t frameIndex = 0;
    render::Fence *pFence;
    size_t fenceValues[kBackBufferCount];

    // display data

    render::Viewport viewport;
    render::Scissor scissor;
    render::DisplayQueue *pDisplayQueue;

    render::DescriptorHeap *pRenderTargetHeap;
    render::RenderTarget *pRenderTargetArray[kBackBufferCount];

    // resources

    render::PipelineState *pPipeline;
    render::VertexBuffer *pVertexBuffer;
    render::IndexBuffer *pIndexBuffer;
};

#define ASSERT(expr) \
    do { \
        if (!(expr)) { \
            simcoe::logError(#expr); \
            std::abort(); \
        } \
    } while (false)

static void commonMain(simcoe::System& system) {
    assets::Assets depot = { std::filesystem::current_path() / "build" / "editor.exe.p" };

    const simcoe::WindowCreateInfo windowCreateInfo = {
        .title = "simcoe",
        .style = simcoe::WindowStyle::eWindowed,
        .width = 800,
        .height = 600
    };

    simcoe::Window window = system.createWindow(windowCreateInfo);

    const RenderCreateInfo renderCreateInfo = {
        .hWindow = window.getHandle(),
        .depot = depot,

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
