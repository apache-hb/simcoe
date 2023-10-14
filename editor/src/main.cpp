#include "engine/engine.h"
#include "engine/os/system.h"

#include "engine/input/win32-device.h"
#include "engine/input/xinput-device.h"
#include "engine/input/gameinput-device.h"

#include "engine/render/graph.h"

#include "editor/graph/assets.h"
#include "editor/graph/post.h"
#include "editor/graph/scene.h"
#include "editor/graph/gui.h"
#include "editor/graph/game.h"

#include "editor/debug/gui.h"

#include "imgui/imgui.h"
#include "imfiles/imfilebrowser.h"

#include "moodycamel/concurrentqueue.h"

#include <functional>
#include <array>
#include <fstream>

using namespace simcoe;
using namespace editor;

namespace gdk = microsoft::gdk;

static constexpr auto kWindowWidth = 1920;
static constexpr auto kWindowHeight = 1080;

/// rendering

static simcoe::System *pSystem = nullptr;
static simcoe::Window *pWindow = nullptr;
static std::jthread *pRenderThread = nullptr;
static render::Graph *pGraph = nullptr;
static bool gFullscreen = false;
static std::atomic_bool gRunning = true;

/// input

static input::Win32Keyboard *pKeyboard = nullptr;
static input::Win32Mouse *pMouse = nullptr;
static input::XInputGamepad *pGamepad0 = nullptr;
//static input::GameInput *pGameInput = nullptr;
static input::Manager *pInput = nullptr;

///
/// projection and view
///
static std::vector<IProjection*> gProjections = {
    new Perspective(),
    new Orthographic(20.f, 20.f)
};

static const char *kProjectionNames[] = { "perspective", "orthographic" };
static int gCurrentProjection = 0;

std::string gGdkFailureReason;

GameLevel gLevel;

static void addObject(std::string name) {
    gLevel.objects.push_back(new EnemyObject(name));
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

struct FileLogger final : ILogSink {
    FileLogger()
        : file("game.log")
    { }

    void accept(std::string_view message) override {
        file << message << std::endl;
    }

    std::ofstream file;
};

struct GuiLogger final : ILogSink {
    void accept(std::string_view message) override {
        buffer.push_back(std::string(message));
    }

    std::vector<std::string> buffer;
};

GuiLogger gGuiLogger;
FileLogger gFileLogger;

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
        pKeyboard->handleMsg(uMsg, wParam, lParam);
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

        if (ImGui::Begin("Input")) {
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

    ImGui::FileBrowser fileBrowser;

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

    char objectName[256] = { 0 };

    void content() override {
        showDockSpace();

        ImGui::ShowDemoWindow();

        if (ImGui::Begin("Scene", &sceneIsOpen)) {
            ISRVHandle *pHandle = pSceneSource->getInner();
            auto offset = ctx->getSrvHeap()->deviceOffset(pHandle->getSrvIndex());
            const auto &createInfo = ctx->getCreateInfo();
            float aspect = float(createInfo.renderWidth) / createInfo.renderHeight;
            ImGui::Image((ImTextureID)offset, { 256 * aspect, 256 });
        }

        ImGui::End();

        ImGui::Begin("Game Objects");

        for (size_t i = 0; i < gLevel.objects.size(); i++) {
            auto *pObject = gLevel.objects[i];
            ImGui::PushID(i);

            ImGui::BulletText("%s", pObject->name.data());

            ImGui::SliderFloat3("position", pObject->position.data(), -10.f, 10.f);
            ImGui::SliderFloat3("rotation", pObject->rotation.data(), -1.f, 1.f);
            ImGui::SliderFloat3("scale", pObject->scale.data(), 0.1f, 10.f);

            ImGui::PopID();
        }

        ImGui::SeparatorText("Add Object");

        ImGui::InputText("name", objectName, std::size(objectName));

        if (ImGui::Button("Add Object")) {
            if (std::strlen(objectName) > 0) {
                addObject(objectName);
                objectName[0] = '\0';
            } else {
                logWarn("cannot add object with no name");
            }
        }

        ImGui::End();

        gInputClient.debugDraw();

        debug::showDebugGui(pGraph);
        showRenderSettings();
        showCameraInfo();
        showGdkInfo();
        showLogInfo();
        showFilePicker();
    }

    static constexpr ImGuiDockNodeFlags kDockFlags = ImGuiDockNodeFlags_PassthruCentralNode;

    static constexpr ImGuiWindowFlags kWindowFlags = ImGuiWindowFlags_MenuBar
                                            | ImGuiWindowFlags_NoCollapse
                                            | ImGuiWindowFlags_NoMove
                                            | ImGuiWindowFlags_NoResize
                                            | ImGuiWindowFlags_NoTitleBar
                                            | ImGuiWindowFlags_NoBackground
                                            | ImGuiWindowFlags_NoBringToFrontOnFocus
                                            | ImGuiWindowFlags_NoNavFocus
                                            | ImGuiWindowFlags_NoDocking;

    void showDockSpace() {
        const ImGuiViewport *pViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(pViewport->WorkPos);
        ImGui::SetNextWindowSize(pViewport->WorkSize);
        ImGui::SetNextWindowViewport(pViewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));

        ImGui::Begin("Editor", nullptr, kWindowFlags);

        ImGui::PopStyleVar(3);

        ImGuiID id = ImGui::GetID("EditorDock");
        ImGui::DockSpace(id, ImVec2(0.f, 0.f), kDockFlags);

        if (ImGui::BeginMenuBar()) {
            ImGui::Text("Editor");
            ImGui::Separator();

            if (ImGui::BeginMenu("File")) {
                ImGui::MenuItem("Save");
                if (ImGui::MenuItem("Open")) {
                    fileBrowser.SetTitle("Open OBJ File");
                    fileBrowser.SetTypeFilters({ ".obj" });
                    fileBrowser.Open();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Style")) {
                if (ImGui::MenuItem("Classic")) {
                    ImGui::StyleColorsClassic();
                }

                if (ImGui::MenuItem("Dark")) {
                    ImGui::StyleColorsDark();
                }

                if (ImGui::MenuItem("Light")) {
                    ImGui::StyleColorsLight();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        ImGui::End();
    }

    void showFilePicker() {
        fileBrowser.Display();

        if (fileBrowser.HasSelected()) {
            auto path = fileBrowser.GetSelected();
            simcoe::logInfo("selected: {}", path.string());
            fileBrowser.ClearSelected();

            addWork("load-obj", [path] {
                auto *pMesh = pGraph->addObject<ObjMesh>(path);
            });
        }
    }

    void showRenderSettings() {
        if (ImGui::Begin("Render Settings")) {
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
        }

        ImGui::End();
    }

    static void showLogInfo() {
        if (ImGui::Begin("Logs")) {
            for (const auto& message : gGuiLogger.buffer) {
                ImGui::Text("%s", message.data());
            }
        }

        ImGui::End();
    }

    static void showCameraInfo() {
        if (ImGui::Begin("Camera")) {
            ImGui::SliderFloat3("position", gLevel.cameraPosition.data(), -10.f, 10.f);
            ImGui::SliderFloat3("rotation", gLevel.cameraRotation.data(), -1.f, 1.f);

            if (ImGui::Combo("projection", &gCurrentProjection, kProjectionNames, std::size(kProjectionNames))) {
                gLevel.pProjection = gProjections[gCurrentProjection];
            }

            ImGui::SliderFloat("fov", &gLevel.fov, 45.f, 120.f);
        }

        ImGui::End();
    }

    static void showGdkInfo() {
        if (ImGui::Begin("GDK")) {
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

using CommandLine = std::vector<std::string>;

CommandLine getCommandLine() {
    CommandLine args;

    int argc;
    auto **argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    for (int i = 0; i < argc; i++) {
        args.push_back(simcoe::util::narrow(argv[i]));
    }

    LocalFree(argv);
    return args;
}

///
/// entry point
///

static void commonMain(const std::filesystem::path& path) {
    GdkInit gdkInit;

    auto assets = path / "editor.exe.p";
    assets::Assets depot = { assets };
    simcoe::logInfo("depot: {}", assets.string());

    const simcoe::WindowCreateInfo windowCreateInfo = {
        .title = "simcoe",
        .style = simcoe::WindowStyle::eWindowed,

        .width = kWindowWidth,
        .height = kWindowHeight,

        .pCallbacks = &gWindowCallbacks
    };

    pWindow = pSystem->createWindow(windowCreateInfo);
    auto [realWidth, realHeight] = pWindow->getSize().as<UINT>(); // if opened in windowed mode the client size will be smaller than the window size

    pInput = new input::Manager();
    // pInput->addSource(pGameInput = new input::GameInput());
    pInput->addSource(pKeyboard = new input::Win32Keyboard());
    pInput->addSource(pMouse = new input::Win32Mouse(pWindow, true));
    pInput->addSource(pGamepad0 = new input::XInputGamepad(0));
    pInput->addClient(&gInputClient);

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

    gLevel.pProjection = gProjections[gCurrentProjection];

    // move the render context into the render thread to prevent hangs on shutdown
    render::Context *pContext = render::Context::create(renderCreateInfo);
    pRenderThread = new std::jthread([pContext](std::stop_token token) {
        setThreadName("render");
        size_t faultCount = 0;
        size_t faultLimit = 3;

        simcoe::Region region("render thread started", "render thread stopped");
        simcoe::logInfo("render fault limit: {} faults", faultLimit);

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
                    faultCount += 1;
                    simcoe::logError("render fault. {} total fault{}", faultCount, faultCount > 1 ? "s" : "");
                    if (faultCount > faultLimit) {
                        simcoe::logError("render thread fault limit reached. exiting");
                        break;
                    }

                    simcoe::logError("exception: {}. attempting to resume", err.what());
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

        gRunning = false;
    });

    std::jthread inputThread([](auto token) {
        setThreadName("input");

        while (!token.stop_requested()) {
            pInput->poll();
        }
    });

    while (pSystem->getEvent()) {
        pSystem->dispatchEvent();
        if (!gRunning) {
            break;
        }
    }
}

static int innerMain() try {
    setThreadName("main");
    simcoe::addSink(&gFileLogger);
    simcoe::addSink(&gGuiLogger);

    char gamePath[1024];
    GetModuleFileNameA(nullptr, gamePath, sizeof(gamePath));
    std::filesystem::path gameDir = gamePath;

    simcoe::logInfo("exe: {}", gamePath);

    // dont use a Region here because we dont want to print `shutdown` if an exception is thrown
    simcoe::logInfo("startup");
    commonMain(gameDir.parent_path());
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
