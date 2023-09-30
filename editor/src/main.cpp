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
    math::float2 uv;
    math::float4 colour;
};

struct FrameData {
    render::RenderTarget *pRenderTarget;
    render::CommandMemory *pMemory;
    size_t fenceValue = 1;
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
        postViewport = {
            .width = static_cast<float>(createInfo.displayWidth),
            .height = static_cast<float>(createInfo.displayHeight),

            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        postScissor = {
            .right = static_cast<LONG>(createInfo.displayWidth),
            .bottom = static_cast<LONG>(createInfo.displayHeight)
        };

        const render::DisplayQueueCreateInfo displayCreateInfo = {
            .hWindow = createInfo.hWindow,
            .width = createInfo.displayWidth,
            .height = createInfo.displayHeight,
            .bufferCount = kBackBufferCount
        };

        pDisplayQueue = pQueue->createDisplayQueue(pContext, displayCreateInfo);
        pRenderTargetHeap = pDevice->createRenderTargetHeap(kBackBufferCount);
        frameIndex = pDisplayQueue->getFrameIndex();

        for (UINT i = 0; i < kBackBufferCount; ++i) {
            render::CommandMemory *pMemory = pDevice->createCommandMemory();
            render::RenderTarget *pRenderTarget = pDisplayQueue->getRenderTarget(i);
            pDevice->mapRenderTarget(pRenderTargetHeap->hostOffset(i), pRenderTarget);

            frameData[i] = { pRenderTarget, pMemory };
        }

        pCommands = pDevice->createCommands(frameData[frameIndex].pMemory);
    }

    void destroyDisplayData() {
        for (UINT i = 0; i < kBackBufferCount; ++i) {
            delete frameData[i].pMemory;
            delete frameData[i].pRenderTarget;
        }

        delete pRenderTargetHeap;
        delete pDisplayQueue;
    }

    void createSceneData() {
        sceneViewport = {
            .width = static_cast<float>(createInfo.renderWidth),
            .height = static_cast<float>(createInfo.renderHeight),

            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        sceneScissor = {
            .right = static_cast<LONG>(createInfo.renderWidth),
            .bottom = static_cast<LONG>(createInfo.renderHeight)
        };

        const render::TextureInfo textureInfo = {
            .width = createInfo.renderWidth,
            .height = createInfo.renderHeight,

            .format = render::PixelFormat::eRGBA8
        };

        pSceneTarget = pDevice->createTexture(textureInfo, render::ResourceState::eRenderTarget);
        pDevice->mapRenderTarget(pRenderTargetHeap->hostOffset(kBackBufferCount), pSceneTarget);
    }

    void destroySceneData() {

    }

    void createResources() {
        // create pso
        const render::PipelineCreateInfo psoCreateInfo = {
            .vertexShader = createInfo.depot.loadBlob("quad.vs.cso"),
            .pixelShader = createInfo.depot.loadBlob("quad.ps.cso"),

            .attributes = {
                { "POSITION", offsetof(Vertex, position), render::TypeFormat::eFloat3 },
                { "TEXCOORD", offsetof(Vertex, uv), render::TypeFormat::eFloat2 }
            },

            .inputs = {
                { render::InputVisibility::ePixel, 0 }
            },

            .samplers = {
                { render::InputVisibility::ePixel, 0 }
            }
        };

        pPipeline = pDevice->createPipelineState(psoCreateInfo);

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

        pVertexBuffer = pDevice->createVertexBuffer(quad.size(), sizeof(Vertex));
        pIndexBuffer = pDevice->createIndexBuffer(indices.size(), render::TypeFormat::eUint16);
        pTextureBuffer = pDevice->createTexture(textureInfo, render::ResourceState::eCopyDest);

        // create texture heap and map texture into it

        pTextureHeap = pDevice->createTextureHeap(1);
        pDevice->mapTexture(pTextureHeap->hostOffset(0), pTextureBuffer);

        // upload data

        pCommands->begin(frameData[frameIndex].pMemory);

        pCommands->copyBuffer(pVertexBuffer, pVertexStaging.get());
        pCommands->copyBuffer(pIndexBuffer, pIndexStaging.get());
        pCommands->copyTexture(pTextureBuffer, pTextureStaging.get(), textureInfo, image.data);

        pCommands->end();

        pQueue->execute(pCommands);
        endFrame();
    }

    void destroyResources() {
        delete pPipeline;
        delete pVertexBuffer;
        delete pIndexBuffer;
    }

    // rendering

    void beginFrame() {
        frameIndex = pDisplayQueue->getFrameIndex();
        render::HostHeapOffset renderTarget = pRenderTargetHeap->hostOffset(frameIndex);
        pCommands->begin(frameData[frameIndex].pMemory);

        pCommands->setPipelineState(pPipeline);
        pCommands->setHeap(pTextureHeap);
        pCommands->setDisplay({ postViewport, postScissor });

        pCommands->transition(frameData[frameIndex].pRenderTarget, render::ResourceState::ePresent, render::ResourceState::eRenderTarget);

        pCommands->setRenderTarget(renderTarget);
        pCommands->clearRenderTarget(renderTarget, { 0.0f, 0.2f, 0.4f, 1.0f });

        pCommands->setShaderInput(pTextureHeap->deviceOffset(0), 0);
        pCommands->setVertexBuffer(pVertexBuffer);
        pCommands->setIndexBuffer(pIndexBuffer);
        pCommands->drawIndexBuffer(6);

        pCommands->transition(frameData[frameIndex].pRenderTarget, render::ResourceState::eRenderTarget, render::ResourceState::ePresent);

        pCommands->end();
    }

    void endFrame() {
        size_t value = frameData[frameIndex].fenceValue++;
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

    FrameData frameData[kBackBufferCount];

    // swapchain resolution dependant data

    render::Viewport postViewport;
    render::Scissor postScissor;

    render::DisplayQueue *pDisplayQueue;

    render::DescriptorHeap *pRenderTargetHeap;

    // scene resolution dependant data

    render::Viewport sceneViewport;
    render::Scissor sceneScissor;

    render::TextureBuffer *pSceneTarget;

    // scene data

    render::DescriptorHeap *pTextureHeap;

    render::PipelineState *pPostPipeline;

    render::VertexBuffer *pScreenQuadVerts;
    render::IndexBuffer *pScreenQuadIndices;

    // resources

    render::PipelineState *pPipeline;
    render::VertexBuffer *pVertexBuffer;
    render::IndexBuffer *pIndexBuffer;
    render::TextureBuffer *pTextureBuffer;
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

    std::jthread renderThread([&](std::stop_token token) {
        simcoe::Region region("render thread started", "render thread stopped");

        while (!token.stop_requested()) {
            pContext->render();
        }
    });

    while (system.getEvent()) {
        system.dispatchEvent();
    }
}

static int innerMain(simcoe::System& system) try {
    // dont use a Region here because we want to know if commonMain throws an exception
    simcoe::logInfo("startup");
    commonMain(system);
    simcoe::logInfo("shutdown");

    return 0;
} catch (const std::exception& err) {
    simcoe::logError(std::format("unhandled exception: {}", err.what()));
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
