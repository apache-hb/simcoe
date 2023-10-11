#include "engine/engine.h"
#include "engine/os/system.h"

#include "engine/input/xinput-device.h"
#include "engine/input/gameinput-device.h"

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

namespace gdk = microsoft::gdk;

static constexpr auto kWindowWidth = 1920;
static constexpr auto kWindowHeight = 1080;

static simcoe::System *pSystem = nullptr;
static simcoe::Window *pWindow = nullptr;
static std::jthread *pRenderThread = nullptr;
static render::Graph *pGraph = nullptr;
static bool gFullscreen = false;

static input::Manager *pInput = nullptr;

std::string gGdkFailureReason;

GameLevel gLevel;

static void addObject(std::string name) {
    gLevel.objects.push_back({
        .name = name
    });
}

using WorkItem = std::function<void()>;
struct WorkMessage {
    std::string_view name;
    WorkItem item;
};

moodycamel::ConcurrentQueue<WorkMessage> gWorkQueue{64};
static std::jthread *pWorkThread = nullptr;

static void addWork(std::string_view name, WorkItem item) {
    WorkMessage message = {
        .name = name,
        .item = item
    };

    gWorkQueue.enqueue(message);
}

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
        addWork("resize-display", [&, event] {
            auto [width, height] = event;

            if (pGraph) pGraph->resizeDisplay(width, height);
            logInfo("resize-display: {}x{}", width, height);
        });
    }

    bool onEvent(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override {
        return graph::IGuiPass::handleMsg(hWnd, uMsg, wParam, lParam);
    }
};

struct GameInputClient final : input::IClient {
    void onInput(const input::State& newState) override {
        writeNewState(newState);
        updates += 1;
    }

    static constexpr ImGuiTableFlags kTableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersH | ImGuiTableFlags_BordersV;

    void debugDraw() {
        auto state = readState();

        ImGui::Begin("input");
        ImGui::Text("updates: %zu", updates.load());
        ImGui::Text("device: %s", input::toString(state.device).data());
        ImGui::SeparatorText("buttons");

        if (ImGui::BeginTable("buttons", 2, kTableFlags)) {
            ImGui::TableNextColumn();
            ImGui::Text("button");
            ImGui::TableNextColumn();
            ImGui::Text("state");

            for (size_t i = 0; i < state.buttons.size(); i++) {
                ImGui::TableNextColumn();
                ImGui::Text("%s", input::toString(input::Button(i)).data());
                ImGui::TableNextColumn();
                ImGui::Text("%zu", state.buttons[i]);
            }

            ImGui::EndTable();
        }

        ImGui::SeparatorText("axes");
        if (ImGui::BeginTable("axes", 2, kTableFlags)) {
            ImGui::TableNextColumn();
            ImGui::Text("axis");
            ImGui::TableNextColumn();
            ImGui::Text("value");

            for (size_t i = 0; i < state.axes.size(); i++) {
                ImGui::TableNextColumn();
                ImGui::Text("%s", input::toString(input::Axis(i)).data());
                ImGui::TableNextColumn();
                ImGui::Text("%f", state.axes[i]);
            }

            ImGui::EndTable();
        }

        ImGui::End();
    }

private:
    void writeNewState(const input::State& newState) {
        std::lock_guard guard(lock);
        state = newState;
    }

    input::State readState() {
        std::lock_guard guard(lock);
        return state;
    }

    std::atomic_size_t updates = 0;

    std::mutex lock;
    std::atomic_size_t buffer = 0;

    input::State state;
};

GameInputClient gInputClient;

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

        gInputClient.debugDraw();

        ImGui::Begin("debug");
        auto *pSrvHeap = ctx->getSrvHeap();
        auto *pRtvHeap = ctx->getRtvHeap();
        auto& rtvAlloc = pRtvHeap->allocator;
        auto& srvAlloc = pSrvHeap->allocator;

        const auto& createInfo = ctx->getCreateInfo();
        ImGui::Text("present: %dx%d", createInfo.displayWidth, createInfo.displayHeight);
        ImGui::Text("render: %dx%d", createInfo.renderWidth, createInfo.renderHeight);
        if (ImGui::Checkbox("fullscreen", &gFullscreen)) {
            addWork("change-fullscreen", [fs = gFullscreen] {
                if (fs) {
                    pGraph->setFullscreen(true);
                    //pWindow->enterFullscreen();
                } else {
                    pGraph->setFullscreen(false);
                    //pWindow->exitFullscreen();
                }
            });
        }

        bool bTearing = ctx->bAllowTearing;
        ImGui::Checkbox("tearing", &bTearing);
        ctx->bAllowTearing = bTearing;

        ImGui::Text("DXGI reported fullscreen: %s", ctx->bReportedFullscreen ? "true" : "false");

        if (ImGui::SliderInt2("render size", renderSize, 64, 4096)) {
            addWork("resize-render", [this] {
                pGraph->resizeRender(renderSize[0], renderSize[1]);
                logInfo("resize-render: {}x{}", renderSize[0], renderSize[1]);
            });
        }

        if (ImGui::SliderInt("backbuffer count", &backBufferCount, 2, 8)) {
            addWork("change-backbuffers", [this] {
                pGraph->changeBackBufferCount(backBufferCount);
                logInfo("change-backbuffer-count: {}", backBufferCount);
            });
        }

        if (ImGui::Combo("device", &currentAdapter, adapterNames.data(), adapterNames.size())) {
            addWork("change-adapter", [this] {
                pGraph->changeAdapter(currentAdapter);
                logInfo("change-adapter: {}", currentAdapter);
            });
        }

        if (ImGui::Button("Remove Device")) {
            ctx->removeDevice();
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

        const auto& resources = graph->resources;
        const auto& passes = graph->passes;

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
                ImGui::BulletText("  %s (expected: %s)", pHandle->getName().data(), toString(pAttachment->getRequiredState()).data());
            }
        }

        ImGui::End();

        drawGdkInfo();
    }

    static void drawGdkInfo() {
        ImGui::Begin("GDK");
        if (!gdk::enabled()) {
            ImGui::Text("GDK init failed: %s", gGdkFailureReason.c_str());
            ImGui::End();
            return;
        }

        auto info = gdk::getAnalyticsInfo();
        auto id = gdk::getConsoleId();
        const auto& features = gdk::getFeatures();

        auto [osMajor, osMinor, osBuild, osRevision] = info.osVersion;
        ImGui::Text("os: %u.%u.%u - %u", osMajor, osMinor, osBuild, osRevision);
        auto [hostMajor, hostMinor, hostBuild, hostRevision] = info.hostingOsVersion;
        ImGui::Text("host: %u.%u.%u - %u", hostMajor, hostMinor, hostBuild, hostRevision);
        ImGui::Text("family: %s", info.family);
        ImGui::Text("form: %s", info.form);
        ImGui::Text("id: %s", id.data());

        ImGui::SeparatorText("features");

        if (ImGui::BeginTable("features", 2)) {
            ImGui::TableNextColumn();
            ImGui::Text("name");
            ImGui::TableNextColumn();
            ImGui::Text("enabled");

            for (const auto& [name, enabled] : features) {
                ImGui::TableNextColumn();
                ImGui::Text("%s", name.data());
                ImGui::TableNextColumn();
                ImGui::Text("%s", enabled ? "true" : "false");
            }
            ImGui::EndTable();
        }

        ImGui::End();
    }
};

static GameWindow gWindowCallbacks;

struct GdkInit {
    GdkInit() {
       gGdkFailureReason = gdk::init();
    }

    ~GdkInit() {
        gdk::deinit();
    }
};

///
/// entry point
///

static void commonMain() {
    GdkInit gdkInit;

    assets::Assets depot = { std::filesystem::current_path() / "build" / "editor.exe.p" };
    pInput = new input::Manager(new input::XInputGamepad(0));
    pInput->addSource(new input::GameInput());
    pInput->addClient(&gInputClient);

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

        .adapterIndex = 0,
        .backBufferCount = 2,

        .displayWidth = realWidth,
        .displayHeight = realHeight,

        .renderWidth = 1920 * 2,
        .renderHeight = 1080 * 2
    };

    pWorkThread = new std::jthread([](std::stop_token token) {
        setThreadName("work");

        while (!token.stop_requested()) {
            WorkMessage item;
            if (gWorkQueue.try_dequeue(item)) {
                item.item();
            }
        }
        simcoe::logInfo("work thread stopped");
    });

    addObject("jeff");
    addObject("bob");

    // move the render context into the render thread to prevent hangs on shutdown
    render::Context *pContext = render::Context::create(renderCreateInfo);
    pRenderThread = new std::jthread([pContext](std::stop_token token) {
        setThreadName("render");

        simcoe::Region region("render thread started", "render thread stopped");

        // TODO: this is a little stupid
        try {
            pGraph = new Graph(pContext);
            auto *pBackBuffers = pGraph->addResource<graph::SwapChainHandle>();
            auto *pSceneTarget = pGraph->addResource<graph::SceneTargetHandle>();
            auto *pDepthTarget = pGraph->addResource<graph::DepthTargetHandle>();
            auto *pTexture = pGraph->addResource<graph::TextureHandle>("uv-coords.png");
            auto *pUniform = pGraph->addResource<graph::SceneUniformHandle>();

            const graph::GameRenderInfo gameRenderConfig = {
                .pPlayerTexture =  pGraph->addResource<graph::TextureHandle>("player.png"),
                .pCameraUniform = pGraph->addResource<graph::CameraUniformHandle>(),
                .pPlayerMesh = pGraph->addObject<ObjMesh>("G:\\untitled.obj")
            };

            pGraph->addPass<graph::ScenePass>(pSceneTarget->as<IRTVHandle>(), pTexture, pUniform);
            pGraph->addPass<graph::GameLevelPass>(&gLevel, pSceneTarget->as<IRTVHandle>(), pDepthTarget->as<IDSVHandle>(), gameRenderConfig);
            pGraph->addPass<graph::PostPass>(pBackBuffers->as<IRTVHandle>(), pSceneTarget->as<ISRVHandle>());
            pGraph->addPass<GameGui>(pBackBuffers->as<IRTVHandle>(), pSceneTarget->as<ISRVHandle>());
            pGraph->addPass<graph::PresentPass>(pBackBuffers);

            // TODO: if the render loop throws an exception, the program will std::terminate
            // we should handle this case and restart the render loop
            while (!token.stop_requested()) {
                try {
                    pGraph->execute();
                } catch (std::runtime_error& err) {
                    simcoe::logError("render thread exception: {}. attempting to resume", err.what());
                    pGraph->resumeFromFault();
                } catch (...) {
                    simcoe::logError("unknown thread exception. exiting");
                    break;
                }
            }

            delete pGraph;
            delete pContext;
        } catch (std::runtime_error& err) {
            simcoe::logError("render thread exception during startup: {}", err.what());
        }
    });

    std::jthread inputThread([](auto token) {
        setThreadName("input");

        while (!token.stop_requested()) {
            pInput->poll();
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
