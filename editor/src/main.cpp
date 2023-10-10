#include "engine/engine.h"
#include "engine/os/system.h"

#include "engine/render/graph.h"

#include "editor/graph/assets.h"
#include "editor/graph/post.h"
#include "editor/graph/scene.h"
#include "editor/graph/gui.h"
#include "editor/graph/game.h"

#include "imgui/imgui.h"

#include "moodycamel/concurrentqueue.h"

#include <functional>
#include <array>

using namespace simcoe;
using namespace editor;

static constexpr auto kWindowWidth = 1920;
static constexpr auto kWindowHeight = 1080;

static simcoe::System *pSystem = nullptr;
static simcoe::Window *pWindow = nullptr;
static std::jthread *pRenderThread = nullptr;
static render::Graph *pGraph = nullptr;
static bool gFullscreen = false;

GameLevel gLevel;

static void addObject(std::string name) {
    gLevel.objects.push_back({
        .name = name
    });
}

using WorkItem = std::function<void()>;

moodycamel::ConcurrentQueue<WorkItem> gWorkQueue{64};
static std::jthread *pWorkThread = nullptr;

using graph::Vertex;

struct CubeMesh final : ISingleMeshBufferHandle {
    CubeMesh(Graph *ctx)
        : ISingleMeshBufferHandle(ctx, "mesh.cube")
    { }

    // z-up

    static constexpr auto kCubeVerts = std::to_array<Vertex>({
        // top
        Vertex { { -1.f, -1.f, 1.f }, { 0.f, 0.f } }, // top back left
        Vertex { { 1.f, -1.f, 1.f }, { 1.f, 0.f } }, // top back right
        Vertex { { -1.f, 1.f, 1.f }, { 0.f, 1.f } }, // top front left
        Vertex { { 1.f, 1.f, 1.f }, { 1.f, 1.f } }, // top front right

        // bottom
        Vertex { { -1.f, -1.f, -1.f }, { 0.f, 0.f } }, // bottom back left
        Vertex { { 1.f, -1.f, -1.f }, { 1.f, 0.f } }, // bottom back right
        Vertex { { -1.f, 1.f, -1.f }, { 0.f, 1.f } }, // bottom front left
        Vertex { { 1.f, 1.f, -1.f }, { 1.f, 1.f } }, // bottom front right
    });

#define FACE(a, b, c, d) a, b, c, c, b, d

    static constexpr auto kCubeIndices = std::to_array<uint16_t>({
        // top face
        FACE(0, 2, 1, 3),

        // bottom face
        FACE(4, 5, 6, 7),

        // front face
        FACE(2, 3, 6, 7),

        // back face
        FACE(0, 1, 4, 5),

        // left face
        FACE(0, 2, 4, 6),

        // right face
        FACE(1, 3, 5, 7)
    });

    size_t getIndexCount() const override {
        return kCubeIndices.size();
    }

    std::vector<rhi::VertexAttribute> getVertexAttributes() const override {
        return {
            { "POSITION", offsetof(Vertex, position), rhi::TypeFormat::eFloat3 },
            { "TEXCOORD", offsetof(Vertex, uv), rhi::TypeFormat::eFloat2 }
        };
    }

    void create() override {
        std::unique_ptr<rhi::UploadBuffer> pVertexStaging{ctx->createUploadBuffer(kCubeVerts.data(), kCubeVerts.size() * sizeof(Vertex))};
        std::unique_ptr<rhi::UploadBuffer> pIndexStaging{ctx->createUploadBuffer(kCubeIndices.data(), kCubeIndices.size() * sizeof(uint16_t))};

        auto *pVertexBuffer = ctx->createVertexBuffer(kCubeVerts.size(), sizeof(Vertex));
        auto *pIndexBuffer = ctx->createIndexBuffer(kCubeIndices.size(), rhi::TypeFormat::eUint16);

        ctx->beginCopy();
        ctx->copyBuffer(pVertexBuffer, pVertexStaging.get());
        ctx->copyBuffer(pIndexBuffer, pIndexStaging.get());
        ctx->endCopy();

        setIndexBuffer(pIndexBuffer);
        setVertexBuffer(pVertexBuffer);
    }

    void destroy() override {
        delete getIndexBuffer();
        delete getVertexBuffer();
    }
};

struct GameWindow final : IWindowCallbacks {
    void onClose() override {
        delete pWorkThread;
        delete pRenderThread;
        pSystem->quit();
    }

    void onResize(const ResizeEvent& event) override {
        gWorkQueue.enqueue([&, event] {
            auto [width, height] = event;

            if (pGraph) pGraph->resizeDisplay(width, height);
            logInfo("resize-display: {}x{}", width, height);
        });
    }

    void onFullscreen(bool bFullscreen) override {
        gWorkQueue.enqueue([bFullscreen] {
            if (pGraph) pGraph->setFullscreen(bFullscreen);
            gFullscreen = bFullscreen;
            logInfo("set-fullscreen: {}", bFullscreen);
        });
    }

    bool onEvent(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override {
        return graph::IGuiPass::handleMsg(hWnd, uMsg, wParam, lParam);
    }
};

struct GameGui final : graph::IGuiPass {
    using graph::IGuiPass::IGuiPass;

    int renderSize[2];
    int backBufferCount;

    int currentAdapter = 0;
    std::vector<const char*> adapterNames;

    PassAttachment<ISRVHandle> *pSceneSource = nullptr;

    GameGui(Graph *ctx, ResourceWrapper<IRTVHandle> *pRenderTarget, ResourceWrapper<ISRVHandle> *pSceneSource)
        : IGuiPass(ctx, pRenderTarget)
        , pSceneSource(addAttachment(pSceneSource, rhi::ResourceState::eShaderResource))
    { }

    void create() override {
        IGuiPass::create();

        const auto &createInfo = ctx->getCreateInfo();
        renderSize[0] = createInfo.renderWidth;
        renderSize[1] = createInfo.renderHeight;

        backBufferCount = createInfo.backBufferCount;

        currentAdapter = createInfo.adapterIndex;
        for (auto *pAdapter : ctx->getAdapters()) {
            auto info = pAdapter->getInfo();
            adapterNames.push_back(_strdup(info.name.c_str()));
        }
    }

    void destroy() override {
        IGuiPass::destroy();

        for (const char *pName : adapterNames) {
            free((void*)pName);
        }

        adapterNames.clear();
    }

    static constexpr const char *stateToString(rhi::ResourceState state) {
        switch (state) {
        case rhi::ResourceState::ePresent: return "present";
        case rhi::ResourceState::eRenderTarget: return "render-target";
        case rhi::ResourceState::eShaderResource: return "shader-resource";
        case rhi::ResourceState::eCopyDest: return "copy-dest";
        case rhi::ResourceState::eDepthWrite: return "depth-write";

        default: return "unknown";
        }
    }

    bool sceneIsOpen = true;

    void content() override {
        ImGui::ShowDemoWindow();

        if (ImGui::Begin("scene", &sceneIsOpen)) {
            ISRVHandle *pHandle = pSceneSource->getInner();
            auto offset = ctx->getSrvHeap()->deviceOffset(pHandle->getSrvIndex());
            const auto &createInfo = ctx->getCreateInfo();
            float aspect = float(createInfo.renderWidth) / createInfo.renderHeight;
            ImGui::Image((ImTextureID)offset, { 256 * aspect, 256 });
        }

        ImGui::End();

        ImGui::Begin("level");
        for (size_t i = 0; i < gLevel.objects.size(); i++) {
            auto& object = gLevel.objects[i];
            ImGui::PushID(i);

            ImGui::BulletText("%s", object.name.data());

            ImGui::SliderFloat3("position", object.position.data(), -10.f, 10.f);
            ImGui::SliderFloat3("rotation", object.rotation.data(), -1.f, 1.f);
            ImGui::SliderFloat3("scale", object.scale.data(), 0.1f, 10.f);

            ImGui::PopID();
        }

        ImGui::Separator();

        ImGui::SliderFloat3("camera position", gLevel.cameraPosition.data(), -10.f, 10.f);

        ImGui::End();

        ImGui::Begin("debug");
        auto *pSrvHeap = ctx->getSrvHeap();
        auto *pRtvHeap = ctx->getRtvHeap();
        auto& rtvAlloc = pRtvHeap->allocator;
        auto& srvAlloc = pSrvHeap->allocator;

        const auto& createInfo = ctx->getCreateInfo();
        ImGui::Text("present: %dx%d", createInfo.displayWidth, createInfo.displayHeight);
        ImGui::Text("render: %dx%d", createInfo.renderWidth, createInfo.renderHeight);
        if (ImGui::Checkbox("fullscreen", &gFullscreen)) {
            gWorkQueue.enqueue([fs = gFullscreen] {
                if (fs) {
                    pGraph->setFullscreen(true);
                    pWindow->enterFullscreen();
                } else {
                    pGraph->setFullscreen(false);
                    pWindow->exitFullscreen();
                }
            });
        }

        ImGui::Checkbox("tearing", &ctx->bAllowTearing);
        ImGui::Text("DXGI reported fullscreen: %s", ctx->bReportedFullscreen ? "true" : "false");

        if (ImGui::SliderInt2("render size", renderSize, 64, 4096)) {
            gWorkQueue.enqueue([this] {
                pGraph->resizeRender(renderSize[0], renderSize[1]);
                logInfo("resize-render: {}x{}", renderSize[0], renderSize[1]);
            });
        }

        if (ImGui::SliderInt("backbuffer count", &backBufferCount, 2, 8)) {
            gWorkQueue.enqueue([this] {
                pGraph->changeBackBufferCount(backBufferCount);
                logInfo("change-backbuffer-count: {}", backBufferCount);
            });
        }

        if (ImGui::Combo("device", &currentAdapter, adapterNames.data(), adapterNames.size())) {
            gWorkQueue.enqueue([this] {
                pGraph->changeAdapter(currentAdapter);
                logInfo("change-adapter: {}", currentAdapter);
            });
        }

        ImGui::Text("rtv heap: %zu", rtvAlloc.getSize());
        for (size_t i = 0; i < rtvAlloc.getSize(); i++) {
            ImGui::BulletText("%zu: %s", i, rtvAlloc.test(simcoe::BitMap::Index(i)) ? "used" : "free");
        }

        ImGui::Text("srv heap: %zu", srvAlloc.getSize());
        for (size_t i = 0; i < srvAlloc.getSize(); i++) {
            ImGui::BulletText("%zu: %s", i, srvAlloc.test(simcoe::BitMap::Index(i)) ? "used" : "free");
        }

        ImGui::End();

        ImGui::Begin("render");

        const auto& resources = pGraph->resources;
        const auto& passes = pGraph->passes;

        ImGui::Text("resources %zu", resources.size());
        for (auto *pResource : resources) {
            auto name = pResource->getName();
            ImGui::BulletText("%s", name.data());
        }

        ImGui::Text("passes: %zu", passes.size());
        for (auto *pPass : passes) {
            auto name = pPass->getName();
            ImGui::BulletText("%s", name.data());
            for (auto *pAttachment : pPass->inputs) {
                auto *pHandle = pAttachment->getResourceHandle();
                ImGui::BulletText("  %s (expected: %s)", pHandle->getName().data(), stateToString(pAttachment->getRequiredState()));
            }
        }

        ImGui::End();
    }
};

static GameWindow gWindowCallbacks;

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

    pWindow = pSystem->createWindow(windowCreateInfo);
    auto [realWidth, realHeight] = pWindow->getSize().as<UINT>(); // if opened in windowed mode the client size will be smaller than the window size

    const render::RenderCreateInfo renderCreateInfo = {
        .hWindow = pWindow->getHandle(),
        .depot = depot,

        .displayWidth = realWidth,
        .displayHeight = realHeight,

        .renderWidth = 1920 * 2,
        .renderHeight = 1080 * 2
    };

    pWorkThread = new std::jthread([](std::stop_token token) {
        while (!token.stop_requested()) {
            WorkItem item;
            if (gWorkQueue.try_dequeue(item)) {
                item();
            }
        }
        simcoe::logInfo("work thread stopped");
    });

    addObject("jeff");
    addObject("bob");

    // move the render context into the render thread to prevent hangs on shutdown
    render::Context *pContext = render::Context::create(renderCreateInfo);
    pRenderThread = new std::jthread([pContext](std::stop_token token) {
        simcoe::Region region("render thread started", "render thread stopped");

        pGraph = new Graph(pContext);
        try {
            auto *pBackBuffers = pGraph->addResource<graph::SwapChainHandle>();
            auto *pSceneTarget = pGraph->addResource<graph::SceneTargetHandle>();
            auto *pDepthTarget = pGraph->addResource<graph::DepthTargetHandle>();
            auto *pTexture = pGraph->addResource<graph::TextureHandle>("uv-coords.png");
            auto *pUniform = pGraph->addResource<graph::SceneUniformHandle>();

            auto *pPlayerMesh = pGraph->addObject<ObjMesh>("G:\\untitled.obj");

            const graph::GameRenderInfo gameRenderConfig = {
                .pPlayerTexture =  pTexture, //pGraph->addResource<graph::TextureHandle>("player.png"),
                .pCameraUniform = pGraph->addResource<graph::CameraUniformHandle>(),
                .pPlayerMesh = pPlayerMesh
            };

            pGraph->addPass<graph::ScenePass>(pSceneTarget->as<IRTVHandle>(), pTexture, pUniform);
            pGraph->addPass<graph::GameLevelPass>(&gLevel, pSceneTarget->as<IRTVHandle>(), pDepthTarget->as<IDSVHandle>(), gameRenderConfig);
            pGraph->addPass<graph::PostPass>(pSceneTarget->as<ISRVHandle>(), pBackBuffers->as<IRTVHandle>());
            pGraph->addPass<GameGui>(pBackBuffers->as<IRTVHandle>(), pSceneTarget->as<ISRVHandle>());
            pGraph->addPass<graph::PresentPass>(pBackBuffers->as<IRTVHandle>());

            // TODO: if the render loop throws an exception, the program will std::terminate
            // we should handle this case and restart the render loop
            while (!token.stop_requested()) {
                pGraph->execute();
            }

            delete pGraph;
            delete pContext;
        } catch (std::runtime_error& err) {
            simcoe::logError("render thread exception: {}", err.what());
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
