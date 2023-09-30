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

template<typename T>
struct DescriptorAlloc {
    enum struct Index : size_t { };

    DescriptorAlloc(render::DescriptorHeap *pHeap)
        : pHeap(pHeap)
        , offset(0)
    { }

    ~DescriptorAlloc() {
        delete pHeap;
    }

    Index alloc() {
        return Index(offset++);
    }

    render::HostHeapOffset hostOffset(Index index) const {
        return pHeap->hostOffset(size_t(index));
    }

    render::DeviceHeapOffset deviceOffset(Index index) const {
        return pHeap->deviceOffset(size_t(index));
    }

    render::DescriptorHeap *pHeap;
    size_t offset;
};

struct RenderTargetHeap;
struct TextureHeap;

using RenderTargetAlloc = DescriptorAlloc<RenderTargetHeap>;
using TextureAlloc = DescriptorAlloc<TextureHeap>;

struct FrameData {
    render::RenderTarget *pRenderTarget;
    RenderTargetAlloc::Index renderTargetHeapIndex;

    render::CommandMemory *pMemory;
    size_t fenceValue = 1;
};

struct RenderContext {
    static constexpr UINT kBackBufferCount = 2;
    static constexpr math::float4 kClearColour = { 0.0f, 0.2f, 0.4f, 1.0f };
    static constexpr math::float4 kBlackClearColour = { 0.0f, 0.0f, 0.0f, 1.0f };

    static RenderContext *create(const RenderCreateInfo& createInfo) {
        return new RenderContext(createInfo);
    }

    ~RenderContext() {
        destroyResources();
        destroyPostData();
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
        createPostData();
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
        const render::DisplayQueueCreateInfo displayCreateInfo = {
            .hWindow = createInfo.hWindow,
            .width = createInfo.displayWidth,
            .height = createInfo.displayHeight,
            .bufferCount = kBackBufferCount
        };

        pDisplayQueue = pQueue->createDisplayQueue(pContext, displayCreateInfo);
        render::DescriptorHeap *pHeap = pDevice->createRenderTargetHeap(kBackBufferCount + 1);
        pRenderTargetAlloc = new RenderTargetAlloc(pHeap);

        frameIndex = pDisplayQueue->getFrameIndex();

        for (UINT i = 0; i < kBackBufferCount; ++i) {
            render::CommandMemory *pMemory = pDevice->createCommandMemory();
            render::RenderTarget *pRenderTarget = pDisplayQueue->getRenderTarget(i);
            RenderTargetAlloc::Index rtvIndex = pRenderTargetAlloc->alloc();

            pDevice->mapRenderTarget(pRenderTargetAlloc->hostOffset(rtvIndex), pRenderTarget);

            frameData[i] = { pRenderTarget, rtvIndex, pMemory };
        }

        pCommands = pDevice->createCommands(frameData[frameIndex].pMemory);
    }

    void destroyDisplayData() {
        for (UINT i = 0; i < kBackBufferCount; ++i) {
            delete frameData[i].pMemory;
            delete frameData[i].pRenderTarget;
        }

        delete pRenderTargetAlloc;
        delete pDisplayQueue;
    }

    void createPostData() {
        // create texture heap

        render::DescriptorHeap *pHeap = pDevice->createTextureHeap(2);
        pTextureAlloc = new TextureAlloc(pHeap);

        // create scissors and viewports
        sceneDisplay = createDisplay(createInfo.renderWidth, createInfo.renderHeight);
        postDisplay = createLetterBoxDisplay();

        // create scene target
        const render::TextureInfo textureInfo = {
            .width = createInfo.renderWidth,
            .height = createInfo.renderHeight,

            .format = render::PixelFormat::eRGBA8
        };

        pSceneTarget = pDevice->createTextureRenderTarget(textureInfo, kClearColour);
        sceneRenderTargetIndex = pRenderTargetAlloc->alloc();
        screenTextureIndex = pTextureAlloc->alloc();

        pDevice->mapRenderTarget(pRenderTargetAlloc->hostOffset(sceneRenderTargetIndex), pSceneTarget);
        pDevice->mapTexture(pTextureAlloc->hostOffset(screenTextureIndex), pSceneTarget);

        const render::PipelineCreateInfo psoCreateInfo = {
            .vertexShader = createInfo.depot.loadBlob("blit.vs.cso"),
            .pixelShader = createInfo.depot.loadBlob("blit.ps.cso"),

            .attributes = {
                { "POSITION", offsetof(Vertex, position), render::TypeFormat::eFloat3 },
                { "TEXCOORD", offsetof(Vertex, uv), render::TypeFormat::eFloat2 }
            },

            .inputs = {
                { render::InputVisibility::ePixel, 0, false }
            },

            .samplers = {
                { render::InputVisibility::ePixel, 0 }
            }
        };

        pPostPipeline = pDevice->createPipelineState(psoCreateInfo);

        const auto quad = std::to_array<Vertex>({
            Vertex{ { 1.f, -1.f, 0.0f }, { 0.f, 0.f } }, // top left
            Vertex{ { 1.f, 1.f, 0.0f }, { 1.f, 0.f } }, // top right
            Vertex{ { -1.f, -1.f, 0.0f }, { 0.f, 1.f } }, // bottom left
            Vertex{ { -1.f, 1.f, 0.0f }, { 1.f, 1.f } } // bottom right
        });

        const auto indices = std::to_array<uint16_t>({
            0, 2, 1,
            1, 2, 3
        });

        std::unique_ptr<render::UploadBuffer> pVertexStaging{pDevice->createUploadBuffer(quad.data(), quad.size() * sizeof(Vertex))};
        std::unique_ptr<render::UploadBuffer> pIndexStaging{pDevice->createUploadBuffer(indices.data(), indices.size() * sizeof(uint16_t))};

        pScreenQuadVerts = pDevice->createVertexBuffer(quad.size(), sizeof(Vertex));
        pScreenQuadIndices = pDevice->createIndexBuffer(indices.size(), render::TypeFormat::eUint16);

        pCommands->begin(frameData[frameIndex].pMemory);

        pCommands->copyBuffer(pScreenQuadVerts, pVertexStaging.get());
        pCommands->copyBuffer(pScreenQuadIndices, pIndexStaging.get());

        pCommands->end();

        pQueue->execute(pCommands);
        endFrame();
    }

    void destroyPostData() {
        delete pTextureAlloc;
        delete pPostPipeline;
        delete pScreenQuadVerts;
        delete pScreenQuadIndices;
        delete pSceneTarget;
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
                { render::InputVisibility::ePixel, 0, true }
            },

            .samplers = {
                { render::InputVisibility::ePixel, 0 }
            }
        };

        pScenePipeline = pDevice->createPipelineState(psoCreateInfo);

        // data to upload
        float aspect = float(createInfo.renderHeight) / float(createInfo.renderWidth);

        const auto quad = std::to_array<Vertex>({
            Vertex{ { 0.5f, -0.5f * aspect, 0.0f }, { 0.f, 0.f } }, // top left
            Vertex{ { 0.5f, 0.5f * aspect, 0.0f }, { 1.f, 0.f } }, // top right
            Vertex{ { -0.5f, -0.5f * aspect, 0.0f }, { 0.f, 1.f } }, // bottom left
            Vertex{ { -0.5f, 0.5f * aspect, 0.0f }, { 1.f, 1.f } } // bottom right
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
        pTextureBuffer = pDevice->createTexture(textureInfo);

        // create texture heap and map texture into it

        quadTextureIndex = pTextureAlloc->alloc();
        pDevice->mapTexture(pTextureAlloc->hostOffset(quadTextureIndex), pTextureBuffer);

        // upload data

        pCommands->begin(frameData[frameIndex].pMemory);

        pCommands->copyBuffer(pQuadVertexBuffer, pVertexStaging.get());
        pCommands->copyBuffer(pQuadIndexBuffer, pIndexStaging.get());
        pCommands->copyTexture(pTextureBuffer, pTextureStaging.get(), textureInfo, image.data);

        pCommands->end();

        pQueue->execute(pCommands);
        endFrame();
    }

    void destroyResources() {
        delete pScenePipeline;
        delete pQuadVertexBuffer;
        delete pQuadIndexBuffer;
    }

    // rendering

    void executeScene() {
        auto renderTargetIndex = pRenderTargetAlloc->hostOffset(sceneRenderTargetIndex);

        pCommands->transition(pSceneTarget, render::ResourceState::eShaderResource, render::ResourceState::eRenderTarget);

        // bind state for scene
        pCommands->setPipelineState(pScenePipeline);
        pCommands->setHeap(pTextureAlloc->pHeap);
        pCommands->setDisplay(sceneDisplay);

        pCommands->setRenderTarget(renderTargetIndex);
        pCommands->clearRenderTarget(renderTargetIndex, kClearColour);

        // draw scene content
        pCommands->setShaderInput(pTextureAlloc->deviceOffset(quadTextureIndex), 0);
        pCommands->setVertexBuffer(pQuadVertexBuffer);
        pCommands->setIndexBuffer(pQuadIndexBuffer);
        pCommands->drawIndexBuffer(6);

        pCommands->transition(pSceneTarget, render::ResourceState::eRenderTarget, render::ResourceState::eShaderResource);
    }

    void executePost() {
        auto renderTargetHeapIndex = frameData[frameIndex].renderTargetHeapIndex;
        render::RenderTarget *pRenderTarget = frameData[frameIndex].pRenderTarget;

        pCommands->transition(pRenderTarget, render::ResourceState::ePresent, render::ResourceState::eRenderTarget);

        // bind state for post
        pCommands->setPipelineState(pPostPipeline);
        pCommands->setHeap(pTextureAlloc->pHeap);
        pCommands->setDisplay(postDisplay);

        // set the actual back buffer as the render target
        pCommands->setRenderTarget(pRenderTargetAlloc->hostOffset(renderTargetHeapIndex));
        pCommands->clearRenderTarget(pRenderTargetAlloc->hostOffset(renderTargetHeapIndex), kBlackClearColour);

        // blit scene to backbuffer
        pCommands->setShaderInput(pTextureAlloc->deviceOffset(screenTextureIndex), 0);
        pCommands->setVertexBuffer(pScreenQuadVerts);
        pCommands->setIndexBuffer(pScreenQuadIndices);
        pCommands->drawIndexBuffer(6);

        pCommands->transition(pRenderTarget, render::ResourceState::eRenderTarget, render::ResourceState::ePresent);
    }

    void beginFrame() {
        frameIndex = pDisplayQueue->getFrameIndex();
        pCommands->begin(frameData[frameIndex].pMemory);

        executeScene();
        executePost();
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

    render::Display createLetterBoxDisplay() {
        UINT displayWidth = createInfo.displayWidth;
        UINT displayHeight = createInfo.displayHeight;

        UINT renderHeight = createInfo.renderHeight;
        UINT renderWidth = createInfo.renderWidth;

        auto widthRatio = float(renderWidth) / displayWidth;
        auto heightRatio = float(renderHeight) / displayHeight;

        float x = 1.f;
        float y = 1.f;

        if (widthRatio < heightRatio) {
            x = widthRatio / heightRatio;
        } else {
            y = heightRatio / widthRatio;
        }

        render::Viewport viewport = {
            .x = displayWidth * (1.f - x) / 2.f,
            .y = displayHeight * (1.f - y) / 2.f,
            .width = x * displayWidth,
            .height = y * displayHeight,
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        render::Scissor scissor = {
            LONG(viewport.x),
            LONG(viewport.y),
            LONG(viewport.x + viewport.width),
            LONG(viewport.y + viewport.height)
        };

        return { viewport, scissor };
    }

    render::Display createDisplay(UINT width, UINT height) {
        render::Viewport viewport = {
            .width = float(width),
            .height = float(height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        render::Scissor scissor = {
            .right = LONG(width),
            .bottom = LONG(height)
        };

        return { viewport, scissor };
    }

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

    render::Display postDisplay;

    render::DisplayQueue *pDisplayQueue;

    RenderTargetAlloc *pRenderTargetAlloc;
    TextureAlloc *pTextureAlloc;

    // render resolution dependant data

    render::Display sceneDisplay;

    render::TextureBuffer *pSceneTarget;
    RenderTargetAlloc::Index sceneRenderTargetIndex;
    TextureAlloc::Index screenTextureIndex;

    // present resolution dependant data

    render::PipelineState *pPostPipeline;

    render::VertexBuffer *pScreenQuadVerts;
    render::IndexBuffer *pScreenQuadIndices;

    // resources

    render::PipelineState *pScenePipeline;
    render::VertexBuffer *pQuadVertexBuffer;
    render::IndexBuffer *pQuadIndexBuffer;

    render::TextureBuffer *pTextureBuffer;
    TextureAlloc::Index quadTextureIndex;
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
        .width = 1920,
        .height = 1080
    };

    simcoe::Window window = system.createWindow(windowCreateInfo);

    const RenderCreateInfo renderCreateInfo = {
        .hWindow = window.getHandle(),
        .depot = depot,

        .displayWidth = 1920,
        .displayHeight = 1080,

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
