#include "engine/engine.h"
#include "engine/os/system.h"

#include "editor/render/render.h"
#include "editor/render/graph.h"

#include <array>

using namespace simcoe;
using namespace editor;

#define ASSERT(expr) \
    do { \
        if (!(expr)) { \
            simcoe::logError(#expr); \
            std::abort(); \
        } \
    } while (false)
    
static constexpr auto kWindowWidth = 1920;
static constexpr auto kWindowHeight = 1080;

static simcoe::System *pSystem = nullptr;
static std::jthread *pRenderThread = nullptr;

struct GameWindow final : IWindowCallbacks {
    void onClose() override {
        delete pRenderThread;
        pSystem->quit();
    }

    void onResize(int width, int height) override {
        logInfo("resize: {}x{}", width, height);
    }
};

static GameWindow gWindowCallbacks;

///
/// create display helper functions
///

constexpr render::Display createDisplay(UINT width, UINT height) {
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

constexpr render::Display createLetterBoxDisplay(UINT renderWidth, UINT renderHeight, UINT displayWidth, UINT displayHeight) {
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

///
/// render passes
///

struct SceneTargetHandle final : IResourceHandle {
    static constexpr math::float4 kClearColour = { 0.0f, 0.2f, 0.4f, 1.0f };

    void create(RenderContext *ctx) override {
        const auto& createInfo = ctx->getCreateInfo();

        const render::TextureInfo textureCreateInfo = {
            .width = createInfo.renderWidth,
            .height = createInfo.renderHeight,
            .format = render::PixelFormat::eRGBA8,
        };

        pResource = ctx->createTextureRenderTarget(textureCreateInfo, kClearColour);
        currentState = render::ResourceState::eShaderResource;
        rtvIndex = ctx->mapRenderTarget(pResource);
        srvIndex = ctx->mapTexture(pResource);
    }

    void destroy(RenderContext *ctx) override {
        delete pResource;
    }

    render::DeviceResource* getResource() const override {
        return pResource;
    }

    render::TextureBuffer *pResource;
};

struct TextureHandle final : IResourceHandle {
    TextureHandle(std::string name)
        : name(name)
    { }

    void create(RenderContext *ctx) override {
        const auto& createInfo = ctx->getCreateInfo();
        assets::Image image = createInfo.depot.loadImage(name);

        const render::TextureInfo textureInfo = {
            .width = image.width,
            .height = image.height,

            .format = render::PixelFormat::eRGBA8
        };

        std::unique_ptr<render::UploadBuffer> pTextureStaging{ctx->createTextureUploadBuffer(textureInfo)};
        pResource = ctx->createTexture(textureInfo);
        srvIndex = ctx->mapTexture(pResource);
        currentState = render::ResourceState::eCopyDest;

        ctx->beginCopy();
        ctx->copyTexture(pResource, pTextureStaging.get(), textureInfo, image.data);
        ctx->endCopy();
    }

    void destroy(RenderContext *ctx) override {
        delete pResource;
    }

    render::DeviceResource *getResource() const override {
        return pResource;
    }

    std::string name;
    render::TextureBuffer *pResource;
};

struct UNIFORM_ALIGN UniformData {
    math::float2 offset;

    float angle;
    float aspect;
};

struct ScenePass final : IRenderPass {
    static constexpr auto kQuadVerts = std::to_array<Vertex>({
        Vertex{ { 0.5f, -0.5f, 0.0f }, { 0.f, 0.f } }, // top left
        Vertex{ { 0.5f, 0.5f, 0.0f }, { 1.f, 0.f } }, // top right
        Vertex{ { -0.5f, -0.5f, 0.0f }, { 0.f, 1.f } }, // bottom left
        Vertex{ { -0.5f, 0.5f, 0.0f }, { 1.f, 1.f } } // bottom right
    });

    static constexpr auto kQuadIndices = std::to_array<uint16_t>({
        0, 2, 1,
        1, 2, 3
    });

    ScenePass(SceneTargetHandle *pSceneTarget, TextureHandle *pTexture) 
        : IRenderPass()
        , pSceneTarget(addResource<SceneTargetHandle>(pSceneTarget, render::ResourceState::eRenderTarget))
        , pTextureHandle(addResource<TextureHandle>(pTexture, render::ResourceState::eShaderResource))
    { }

    void create(RenderContext *ctx) override {
        const auto& createInfo = ctx->getCreateInfo();
        pTimer = new simcoe::Timer();
        display = createDisplay(createInfo.renderWidth, createInfo.renderHeight);

        // create pipeline
        const render::PipelineCreateInfo psoCreateInfo = {
            .vertexShader = createInfo.depot.loadBlob("quad.vs.cso"),
            .pixelShader = createInfo.depot.loadBlob("quad.ps.cso"),

            .attributes = {
                { "POSITION", offsetof(Vertex, position), render::TypeFormat::eFloat3 },
                { "TEXCOORD", offsetof(Vertex, uv), render::TypeFormat::eFloat2 }
            },

            .textureInputs = {
                { render::InputVisibility::ePixel, 0, true }
            },

            .uniformInputs = {
                { render::InputVisibility::eVertex, 0, false }
            },

            .samplers = {
                { render::InputVisibility::ePixel, 0 }
            }
        };

        pPipeline = ctx->createPipelineState(psoCreateInfo);


        // create uniform buffer

        pQuadUniformBuffer = ctx->createUniformBuffer(sizeof(UniformData));
        quadUniformIndex = ctx->mapUniform(pQuadUniformBuffer, sizeof(UniformData));
    }

    void destroy(RenderContext *ctx) override {
        delete pPipeline;
        delete pQuadUniformBuffer;

        delete pTimer;
    }

    void execute(RenderContext *ctx) override {
        const auto time = pTimer->now();
        const auto& createInfo = ctx->getCreateInfo();
        UniformData data = {
            .offset = { 0.f, std::sin(time) / 3 },
            .angle = time,
            .aspect = float(createInfo.renderHeight) / float(createInfo.renderWidth)
        };

        pQuadUniformBuffer->write(&data, sizeof(UniformData));

        IResourceHandle *pTarget = pSceneTarget->getHandle();
        IResourceHandle *pTexture = pTextureHandle->getHandle();
        RenderTarget rt = { (render::TextureBuffer*)pTarget->getResource(), pTarget->rtvIndex };

        ctx->setPipeline(pPipeline);
        ctx->setDisplay(display);
        ctx->executeScene(quadUniformIndex, pTexture->srvIndex, rt);
    }

    PassResource<SceneTargetHandle> *pSceneTarget;
    PassResource<TextureHandle> *pTextureHandle;

    simcoe::Timer *pTimer;

    render::Display display;

    render::PipelineState *pPipeline;

    render::UniformBuffer *pQuadUniformBuffer;
    DataAlloc::Index quadUniformIndex;
};

struct PostPass final : IRenderPass {
    static constexpr auto kScreenQuad = std::to_array<Vertex>({
        Vertex{ { 1.f, -1.f, 0.0f }, { 0.f, 0.f } }, // top left
        Vertex{ { 1.f, 1.f, 0.0f }, { 1.f, 0.f } }, // top right
        Vertex{ { -1.f, -1.f, 0.0f }, { 0.f, 1.f } }, // bottom left
        Vertex{ { -1.f, 1.f, 0.0f }, { 1.f, 1.f } } // bottom right
    });

    static constexpr auto kScreenQuadIndices = std::to_array<uint16_t>({
        0, 2, 1,
        1, 2, 3
    });

    PostPass(SceneTargetHandle *pSceneTarget) 
        : IRenderPass()
        , pSceneTarget(addResource<SceneTargetHandle>(pSceneTarget, render::ResourceState::eShaderResource))
    { }
    
    void create(RenderContext *ctx) override {
        const auto& createInfo = ctx->getCreateInfo();

        display = createLetterBoxDisplay(createInfo.renderWidth, createInfo.renderHeight, createInfo.displayWidth, createInfo.displayHeight);

        const render::PipelineCreateInfo psoCreateInfo = {
            .vertexShader = createInfo.depot.loadBlob("blit.vs.cso"),
            .pixelShader = createInfo.depot.loadBlob("blit.ps.cso"),

            .attributes = {
                { "POSITION", offsetof(Vertex, position), render::TypeFormat::eFloat3 },
                { "TEXCOORD", offsetof(Vertex, uv), render::TypeFormat::eFloat2 }
            },

            .textureInputs = {
                { render::InputVisibility::ePixel, 0, false },
            },

            .samplers = {
                { render::InputVisibility::ePixel, 0 }
            }
        };

        pPipeline = ctx->createPipelineState(psoCreateInfo);
    }

    void destroy(RenderContext *ctx) override {
        delete pPipeline;
    }

    void execute(RenderContext *ctx) override {
        IResourceHandle *pTarget = pSceneTarget->getHandle();

        ctx->setPipeline(pPipeline);
        ctx->setDisplay(display);
        ctx->executePost(pTarget->srvIndex);
    }

    PassResource<SceneTargetHandle> *pSceneTarget;

    render::Display display;
    render::PipelineState *pPipeline;
};

struct PresentPass final : IRenderPass {
    void create(RenderContext *ctx) override {

    }

    void destroy(RenderContext *ctx) override {

    }

    void execute(RenderContext *ctx) override {
        ctx->executePresent();
    }
};

///
/// entry point
///

static void commonMain() {
    assets::Assets depot = { std::filesystem::current_path() / "build" / "editor.exe.p" };

    const simcoe::WindowCreateInfo windowCreateInfo = {
        .title = "simcoe",
        .style = simcoe::WindowStyle::eWindowed,

        .width = kWindowWidth,
        .height = kWindowHeight,

        .pCallbacks = &gWindowCallbacks
    };

    simcoe::Window window = pSystem->createWindow(windowCreateInfo);
    auto [width, height] = window.getSize().as<UINT>(); // if opened in windowed mode the client size will be smaller than the window size

    const editor::RenderCreateInfo renderCreateInfo = {
        .hWindow = window.getHandle(),
        .depot = depot,

        .displayWidth = width,
        .displayHeight = height,

        .renderWidth = 1920 * 2,
        .renderHeight = 1080 * 2
    };

    // move the render context into the render thread to prevent hangs on shutdown
    std::unique_ptr<editor::RenderContext> context{editor::RenderContext::create(renderCreateInfo)};
    pRenderThread = new std::jthread([ctx = std::move(context)](std::stop_token token) {
        simcoe::Region region("render thread started", "render thread stopped");
        
        RenderGraph graph{ctx.get()};
        SceneTargetHandle *pSceneTarget = new SceneTargetHandle();
        TextureHandle *pTexture = new TextureHandle("uv-coords.png");

        graph.addResource(pSceneTarget);
        graph.addResource(pTexture);

        graph.addPass(new ScenePass(pSceneTarget, pTexture));
        graph.addPass(new PostPass(pSceneTarget));
        graph.addPass(new PresentPass());

        // TODO: if the render loop throws an exception, the program will std::terminate
        // we should handle this case and restart the render loop
        while (!token.stop_requested()) {
            graph.execute();
        }
    });

    while (pSystem->getEvent()) {
        pSystem->dispatchEvent();
    }
}

static int innerMain() try {
    // dont use a Region here because we dont want to print `shutdown` if an exception is thrown
    simcoe::logInfo("startup");
    commonMain();
    simcoe::logInfo("shutdown");

    return 0;
} catch (const std::exception& err) {
    simcoe::logError("unhandled exception: {}", err.what());
    return 99;
} catch (...) {
    simcoe::logError("unhandled exception");
    return 99;
}

// gui entry point

int wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    pSystem = new simcoe::System(hInstance, nCmdShow);
    return innerMain();
}

// command line entry point

int main(int argc, const char **argv) {
    pSystem = new simcoe::System(GetModuleHandle(nullptr), SW_SHOWDEFAULT);
    return innerMain();
}
