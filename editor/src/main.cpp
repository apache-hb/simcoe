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
        pCommands = pDevice->createCommands();
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
        }
    }

    void createResources() {
        const auto triangle = std::to_array<Vertex>({
            Vertex{ { 0.0f, 0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
            Vertex{ { 0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
            Vertex{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
        });

        pVertexBuffer = pDevice->createVertexBuffer<Vertex>(triangle);

        const render::PipelineCreateInfo psoCreateInfo = {
            .vertexShader = createInfo.depot.load("shaders/triangle.vs.cso"),
            .pixelShader = createInfo.depot.load("shaders/triangle.ps.cso"),

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

    RenderCreateInfo createInfo;

    // device data

    render::Context *pContext;
    std::vector<render::Adapter*> adapters;
    render::Device *pDevice;

    render::DeviceQueue *pQueue;
    render::Commands *pCommands;

    size_t frameIndex = 0;
    render::Fence *pFence;
    size_t fenceValue = 1;

    // display data

    render::DisplayQueue *pDisplayQueue;

    render::DescriptorHeap *pRenderTargetHeap;
    render::RenderTarget *pRenderTargetArray[kBackBufferCount];

    // resources

    render::PipelineState *pPipeline;
    render::VertexBuffer *pVertexBuffer;
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

    HMODULE mod = LoadLibraryA("D3D12Core.dll");
    ASSERT(mod);

    void *pfn = GetProcAddress(mod, "D3D12SDKVersion");
    ASSERT(pfn);

    DWORD *pVersion = reinterpret_cast<DWORD*>(pfn);
    simcoe::logInfo(std::format("D3D12SDKVersion: {}", *pVersion));

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
