// os and system
#include "game/game.h"

// services
#include "engine/service/service.h"
#include "engine/service/debug.h"
#include "engine/service/logging.h"
#include "engine/service/platform.h"
#include "engine/service/freetype.h"
#include "engine/service/gdk.h"

// threads
#include "engine/tasks/task.h"

// input
#include "engine/input/win32-device.h"
#include "engine/input/xinput-device.h"

// render graph
#include "engine/render/graph.h"

// render passes
#include "editor/graph/assets.h"
#include "editor/graph/post.h"
#include "editor/graph/scene.h"
#include "editor/graph/gui.h"
#include "editor/graph/game.h"

// debug gui
#include "editor/debug/debug.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imfiles/imfilebrowser.h"

// vendor
#include "moodycamel/concurrentqueue.h"

// game logic
#include "swarm/input.h"
#include "swarm/levels.h"

#include <functional>
#include <array>
#include <fstream>
#include <iostream>

using namespace simcoe;
using namespace simcoe::math;

using namespace editor;

using namespace game;

namespace fs = std::filesystem;

enum WindowMode : int {
    eModeWindowed,
    eModeBorderless,
    eModeFullscreen,

    eNone
};

// consts
static constexpr auto kWindowWidth = 1920;
static constexpr auto kWindowHeight = 1080;

// system
static game::Instance *pGame = nullptr;

// window mode
static Window *pWindow = nullptr;
static std::atomic_bool bWindowOpen = true;
static WindowMode gWindowMode = eModeWindowed;
static constexpr auto kWindowModeNames = std::to_array({ "Windowed", "Borderless", "Fullscreen" });

/// threads
static tasks::WorkQueue *pMainQueue = nullptr;

/// input
static input::Win32Keyboard *pKeyboard = nullptr;
static input::Win32Mouse *pMouse = nullptr;
static input::XInputGamepad *pGamepad0 = nullptr;
static input::Manager *pInput = nullptr;

/// rendering
static render::Graph *pGraph = nullptr;

template<typename F>
tasks::WorkThread *newTask(const char *name, F&& func) {
    struct WorkImpl final : tasks::WorkThread {
        WorkImpl(const char *name, F&& fn)
            : WorkThread(64, name)
            , fn(std::forward<F>(fn))
        { }

        void run(std::stop_token token) override {
            while (!token.stop_requested()) {
                fn(this, token);
            }
        }

    private:
        F fn;
    };

    return new WorkImpl(name, std::move(func));
}

struct FileLogger final : ISink {
    FileLogger()
        : file("game.log")
    { }

    void accept(std::string_view message) override {
        file << message << std::endl;
    }

    std::ofstream file;
};

struct GuiLogger final : ISink {
    void accept(std::string_view message) override {
        std::lock_guard lock(mutex);
        buffer.push_back(std::string(message));
    }

private:
    std::mutex mutex;
    std::vector<std::string> buffer;

    void debug() {
        std::lock_guard lock(mutex);
        for (const auto& message : buffer) {
            ImGui::Text("%s", message.c_str());
        }
    }

    debug::GlobalHandle debugHandle = debug::addGlobalHandle("Logs", [this] { debug(); });
};

static GuiLogger *pGuiLogger = nullptr;
static FileLogger *pFileLogger = nullptr;

struct GameWindow final : IWindowCallbacks {
    void onClose() override {
        bWindowOpen = false;

        pGame->quit();
    }

    void onResize(const WindowSize& event) override {
        if (!bWindowOpen) return;
        if (!pGame) return;

        pGame->pRenderQueue->add("resize-display", [&, w = event.width, h = event.height] {
            pGraph->resizeDisplay(w, h);
            LOG_INFO("resize-display: {}x{}", w, h);
        });
    }

    bool onEvent(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override {
        pKeyboard->handleMsg(uMsg, wParam, lParam);
        return graph::IGuiPass::handleMsg(hWnd, uMsg, wParam, lParam);
    }
};

static void changeWindowMode(WindowMode oldMode, WindowMode newMode) {
    if (oldMode == newMode) return;
    gWindowMode = newMode;

    if (oldMode == eModeFullscreen) {
        pGraph->setFullscreen(false);
        pWindow->exitFullscreen();
        return;
    }

    switch (newMode) {
    case eModeWindowed:
        pWindow->setStyle(WindowStyle::eWindowed);
        break;
    case eModeBorderless:
        pWindow->setStyle(WindowStyle::eBorderlessFixed);
        break;
    case eModeFullscreen:
        pGraph->setFullscreen(true);
        pWindow->enterFullscreen();
        break;

    default:
        break;
    }
}

struct GameGui final : graph::IGuiPass {
    using graph::IGuiPass::IGuiPass;

    int renderSize[2];
    int backBufferCount;

    int currentAdapter = 0;
    std::vector<const char*> adapterNames;

    ImGui::FileBrowser objFileBrowser;
    ImGui::FileBrowser imguiFileBrowser{ImGuiFileBrowserFlags_EnterNewFilename};

    PassAttachment<ISRVHandle> *pSceneSource = nullptr;

    ResourceWrapper<graph::TextHandle> *pTextHandle = nullptr;
    PassAttachment<graph::TextHandle> *pTextAttachment = nullptr;

    GameGui(Graph *ctx, ResourceWrapper<IRTVHandle> *pRenderTarget, ResourceWrapper<ISRVHandle> *pSceneSource)
        : IGuiPass(ctx, pRenderTarget)
        , pSceneSource(addAttachment(pSceneSource, rhi::ResourceState::eTextureRead))
    {
        pTextHandle = graph->addResource<graph::TextHandle>("SwarmFace-Regular");
        pTextAttachment = addAttachment(pTextHandle, rhi::ResourceState::eTextureRead);
    }

    void sceneDebug() {
        ISRVHandle *pHandle = pSceneSource->getInner();
        auto offset = ctx->getSrvHeap()->deviceOffset(pHandle->getSrvIndex());
        const auto &createInfo = ctx->getCreateInfo();
        float aspect = float(createInfo.renderWidth) / createInfo.renderHeight;

        float availWidth = ImGui::GetWindowWidth() - 32;
        float availHeight = ImGui::GetWindowHeight() - 32;

        float totalWidth = availWidth;
        float totalHeight = availHeight;

        if (availWidth > availHeight * aspect) {
            totalWidth = availHeight * aspect;
        } else {
            totalHeight = availWidth / aspect;
        }

        ImGui::Image((ImTextureID)offset, { totalWidth, totalHeight });
    }

    debug::GlobalHandle debugHandle = debug::addGlobalHandle("Scene", [this] { sceneDebug(); });

    void create() override {
        IGuiPass::create();

        const auto &info = ctx->getCreateInfo();

        renderSize[0] = info.renderWidth;
        renderSize[1] = info.renderHeight;

        backBufferCount = info.backBufferCount;

        currentAdapter = info.adapterIndex;
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

    int eggX = 0;
    int eggY = 0;

    void content() override {
        showDockSpace();

        ImGui::ShowDemoWindow();

        debug::enumGlobalHandles([](auto *pHandle) {
            if (!pHandle->isEnabled()) return;

            if (ImGui::Begin(pHandle->getName())) {
                pHandle->draw();
            }
            ImGui::End();
        });

        showRenderSettings();
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
                if (ImGui::MenuItem("Save ImGui Config")) {
                    imguiFileBrowser.SetTitle("Save ImGui Config");
                    imguiFileBrowser.SetTypeFilters({ ".ini" });
                    imguiFileBrowser.Open();
                }

                if (ImGui::MenuItem("Open")) {
                    objFileBrowser.SetTitle("Open OBJ File");
                    objFileBrowser.SetTypeFilters({ ".obj" });
                    objFileBrowser.Open();
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
        imguiFileBrowser.Display();
        objFileBrowser.Display();

        if (objFileBrowser.HasSelected()) {
            auto path = objFileBrowser.GetSelected();
            LOG_INFO("selected: {}", path.string());
            objFileBrowser.ClearSelected();
        }

        if (imguiFileBrowser.HasSelected()) {
            auto path = imguiFileBrowser.GetSelected();
            LOG_INFO("selected: {}", path.string());
            imguiFileBrowser.ClearSelected();

            ImGui::SaveIniSettingsToDisk(path.string().c_str());
        }
    }

    static void showHeapSlots(bool& open, const char *name, const simcoe::BitMap& alloc) {
        if (open) {
            ImGui::SetNextItemOpen(true);
        }

        if (ImGui::CollapsingHeader(name)) {
            open = true;
            // show a grid of slots
            auto size = alloc.getSize();
            auto rows = size / 8;
            auto cols = size / rows;

            if (ImGui::BeginTable("Slots", cols)) {
                for (auto i = 0; i < size; i++) {
                    ImGui::TableNextColumn();
                    if (alloc.test(BitMap::Index(i))) {
                        ImGui::Text("%d (used)", i);
                    } else {
                        ImGui::TextDisabled("%d (free)", i);
                    }
                }

                ImGui::EndTable();
            }
        } else {
            open = false;
        }
    }

    template<typename T, typename F>
    static void showGraphObjects(bool& open, const char *name, const std::vector<T>& objects, F&& showObject) {
        if (open) {
            ImGui::SetNextItemOpen(true);
        }

        if (ImGui::CollapsingHeader(name)) {
            open = true;
            for (auto& object : objects) {
                showObject(object);
            }
        } else {
            open = false;
        }
    }

    bool bRtvOpen = false;
    bool bSrvOpen = false;
    bool bDsvOpen = false;

    bool bResourcesOpen = false;
    bool bPassesOpen = false;
    bool bObjectsOpen = false;

    void showRenderSettings() {
        if (ImGui::Begin("Render settings")) {
            const auto& createInfo = ctx->getCreateInfo();
            ImGui::Text("Display resolution: %dx%d", createInfo.displayWidth, createInfo.displayHeight);
            ImGui::Text("Internal resolution: %dx%d", createInfo.renderWidth, createInfo.renderHeight);

            int currentWindowMode = gWindowMode;
            if (ImGui::Combo("Window mode", &currentWindowMode, kWindowModeNames.data(), kWindowModeNames.size())) {
                pGame->pRenderQueue->add("change-window-mode", [oldMode = gWindowMode, newMode = currentWindowMode] {
                    changeWindowMode(WindowMode(oldMode), WindowMode(newMode));
                });
            }

            bool bTearing = ctx->bAllowTearing;
            ImGui::Checkbox("Allow tearing", &bTearing);
            ctx->bAllowTearing = bTearing;

            ImGui::Text("DXGI reported fullscreen: %s", ctx->bReportedFullscreen ? "true" : "false");

            if (ImGui::SliderInt2("Internal resolution", renderSize, 64, 4096)) {
                pGame->pRenderQueue->add("resize-render", [this] {
                    pGraph->resizeRender(renderSize[0], renderSize[1]);
                    LOG_INFO("resize-render: {}x{}", renderSize[0], renderSize[1]);
                });
            }

            if (ImGui::SliderInt("backbuffer count", &backBufferCount, 2, 8)) {
                pGame->pRenderQueue->add("change-backbuffers", [this] {
                    pGraph->changeBackBufferCount(backBufferCount);
                    LOG_INFO("change-backbuffer-count: {}", backBufferCount);
                });
            }

            if (ImGui::Combo("Adapter", &currentAdapter, adapterNames.data(), adapterNames.size())) {
                pGame->pRenderQueue->add("change-adapter", [this] {
                    pGraph->changeAdapter(currentAdapter);
                    LOG_INFO("change-adapter: {}", currentAdapter);
                });
            }

            if (ImGui::Button("Remove device")) {
                ctx->removeDevice();
            }

            ImGui::SeparatorText("RenderContext state");
            auto *pRtvHeap = ctx->getRtvHeap();
            auto *pDsvHeap = ctx->getDsvHeap();
            auto *pSrvHeap = ctx->getSrvHeap();

            auto& rtvAlloc = pRtvHeap->allocator;
            auto& dsvAlloc = pDsvHeap->allocator;
            auto& srvAlloc = pSrvHeap->allocator;

            showHeapSlots(bRtvOpen, std::format("RTV heap {}", rtvAlloc.getSize()).c_str(), rtvAlloc);
            showHeapSlots(bDsvOpen, std::format("DSV heap {}", dsvAlloc.getSize()).c_str(), dsvAlloc);
            showHeapSlots(bSrvOpen, std::format("SRV heap {}", srvAlloc.getSize()).c_str(), srvAlloc);

            ImGui::SeparatorText("RenderGraph state");
            const auto& resources = pGraph->resources;
            const auto& passes = pGraph->passes;
            const auto& objects = pGraph->objects;

            showGraphObjects(bResourcesOpen, std::format("resources: {}", resources.size()).c_str(), resources, [](render::IResourceHandle *pResource) {
                auto name = pResource->getName();
                ImGui::Text("%s (state: %s)", name.data(), rhi::toString(pResource->getCurrentState()).data());
            });

            showGraphObjects(bPassesOpen, std::format("passes: {}", passes.size()).c_str(), passes, [](render::ICommandPass *pPass) {
                auto name = pPass->getName();
                ImGui::Text("pass: %s", name.data());
                for (auto& resource : pPass->inputs) {
                    auto handle = resource->getResourceHandle();
                    auto state = resource->getRequiredState();
                    ImGui::BulletText("resource: %s (expected: %s)", handle->getName().data(), rhi::toString(state).data());
                }
            });

            showGraphObjects(bObjectsOpen, std::format("objects: {}", objects.size()).c_str(), objects, [](render::IGraphObject *pObject) {
                auto name = pObject->getName();
                ImGui::Text("%s", name.data());
            });
        }

        ImGui::End();
    }
};

static GameWindow gWindowCallbacks;

void gdkServiceDebug() {
    if (GdkService::getState() & eServiceFaulted) {
        auto failureReason = GdkService::getFailureReason();
        ImGui::Text("GDK init failed: %s", failureReason.data());
        return;
    }

    auto info = GdkService::getAnalyticsInfo();
    auto id = GdkService::getConsoleId();
    const auto& features = GdkService::getFeatures();

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

        for (const auto& [name, bEnabled] : features) {
            ImGui::TableNextColumn();
            ImGui::Text("%s", name.data());
            ImGui::TableNextColumn();
            ImGui::Text("%s", bEnabled ? "enabled" : "disabled");
        }
        ImGui::EndTable();
    }
}

///
/// entry point
///

static void commonMain(const std::filesystem::path& path) {
    debug::GlobalHandle debugHandle = debug::addGlobalHandle("GDK", [] { gdkServiceDebug(); });
    pMainQueue = new tasks::WorkQueue(64);

    auto assets = path / "editor.exe.p";
    assets::Assets depot = { assets };
    LOG_INFO("depot: {}", assets.string());

    const WindowCreateInfo windowCreateInfo = {
        .title = "simcoe",
        .style = WindowStyle::eWindowed,
        .size = { kWindowWidth, kWindowHeight },

        .pCallbacks = &gWindowCallbacks
    };

    pWindow = new Window(windowCreateInfo); // pSystem->createWindow(windowCreateInfo);
    auto [realWidth, realHeight] = pWindow->getSize().as<UINT>(); // if opened in windowed mode the client size will be smaller than the window size

    pInput = new input::Manager();
    pInput->addSource(pKeyboard = new input::Win32Keyboard());
    pInput->addSource(pMouse = new input::Win32Mouse(pWindow, true));
    pInput->addSource(pGamepad0 = new input::XInputGamepad(0));
    pInput->addClient(swarm::getInputClient());

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

    // move the render context into the render thread to prevent hangs on shutdown
    render::Context *pContext = render::Context::create(renderCreateInfo);
    pGraph = new Graph(pContext);
    pGame = new game::Instance(pGraph);
    game::setInstance(pGame);

    // setup render

    pGame->setupRender();

    auto *pBackBuffers = pGraph->addResource<graph::SwapChainHandle>();
    auto *pSceneTarget = pGraph->addResource<graph::SceneTargetHandle>();
    auto *pDepthTarget = pGraph->addResource<graph::DepthTargetHandle>();

    pGraph->addPass<graph::ScenePass>(pSceneTarget->as<IRTVHandle>());

    pGraph->addPass<graph::GameLevelPass>(pSceneTarget->as<IRTVHandle>(), pDepthTarget->as<IDSVHandle>());

    //pGraph->addPass<graph::PostPass>(pBackBuffers->as<IRTVHandle>(), pSceneTarget->as<ISRVHandle>());
    pGraph->addPass<GameGui>(pBackBuffers->as<IRTVHandle>(), pSceneTarget->as<ISRVHandle>());
    pGraph->addPass<graph::PresentPass>(pBackBuffers);

    // setup game

    pGame->setupGame();
    pGame->pushLevel(new swarm::PlayLevel());

    std::jthread inputThread([](auto token) {
        DebugService::setThreadName("input");

        while (!token.stop_requested()) {
            pInput->poll();
        }
    });

    std::jthread gameThread([](auto token) {
        DebugService::setThreadName("game");

        while (!token.stop_requested()) {
            pGame->updateGame();
        }
    });

    std::jthread renderThread([](auto token) {
        DebugService::setThreadName("render");

        while (!token.stop_requested() && bWindowOpen) {
            pGame->updateRender();
        }
    });

    while (!pGame->shouldQuit()) {
        if (PlatformService::getEvent())
            PlatformService::dispatchEvent();

        pMainQueue->process();
    }

    inputThread.request_stop();
    gameThread.request_stop();
    renderThread.request_stop();

    PlatformService::quit();
}

static fs::path getGameDir() {
    char gamePath[1024];
    GetModuleFileNameA(nullptr, gamePath, sizeof(gamePath));
    return fs::path(gamePath).parent_path();
}

static int innerMain() try {
    DebugService::setThreadName("main");

    pFileLogger = LoggingService::newSink<FileLogger>();
    pGuiLogger = LoggingService::newSink<GuiLogger>();

    auto engineServices = std::to_array({
        LoggingService::service(),
        DebugService::service(),
        PlatformService::service(),
        GdkService::service(),
        FreeTypeService::service()
    });
    ServiceRuntime runtime{engineServices};

    // dont use a Region here because we dont want to print `shutdown` if an exception is thrown
    LOG_INFO("startup");
    commonMain(getGameDir());
    LOG_INFO("shutdown");

    return 0;
} catch (const std::exception& err) {
    LOG_ERROR("unhandled exception: {}", err.what());
    return 99;
} catch (...) {
    LOG_ERROR("unhandled exception");
    return 99;
}

// gui entry point

int wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    PlatformService::setup(hInstance, nCmdShow);
    return innerMain();
}

// command line entry point

int main(int argc, const char **argv) {
    PlatformService::setup(GetModuleHandle(nullptr), SW_SHOWDEFAULT);
    return innerMain();
}
