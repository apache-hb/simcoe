// services
#include "engine/service/service.h"
#include "engine/service/debug.h"
#include "engine/service/logging.h"
#include "engine/service/platform.h"
#include "engine/service/freetype.h"

// threads
#include "engine/threads/service.h"
#include "engine/threads/queue.h"

// util
#include "engine/util/time.h"

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
#include "editor/debug/service.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include "imfiles/imfilebrowser.h"
#include "implot/implot.h"

// vendor
#include "vendor/microsoft/gdk.h"
#include "vendor/amd/ryzen.h"

// game
#include "game/world.h"
#include "game/render/hud.h"
#include "game/render/scene.h"

#include <functional>
#include <fstream>
#include <iostream>

using namespace simcoe;
using namespace simcoe::math;

using namespace editor;

using microsoft::GdkService;
using amd::RyzenMonitorSerivce;

namespace gr = game::graph;
namespace sr = simcoe::render;

enum WindowMode : int {
    eModeWindowed,
    eModeBorderless,
    eModeFullscreen,

    eNone
};

// consts
static constexpr auto kWindowWidth = 1920;
static constexpr auto kWindowHeight = 1080;

// game
game::World *pWorld = nullptr;

// window mode
static Window *pWindow = nullptr;
static std::atomic_bool bWindowOpen = true;
static WindowMode gWindowMode = eModeWindowed;
static constexpr auto kWindowModeNames = std::to_array({ "Windowed", "Borderless", "Fullscreen" });

/// threads
static threads::WorkQueue *pMainQueue = nullptr;
static std::vector<std::jthread> workPool; // TODO: use a thread pool

/// input
static input::Win32Keyboard *pKeyboard = nullptr;
static input::Win32Mouse *pMouse = nullptr;
static input::XInputGamepad *pGamepad0 = nullptr;
static input::Manager *pInput = nullptr;

/// rendering
static sr::Context *pContext = nullptr;
static sr::Graph *pGraph = nullptr;

template<typename F>
threads::WorkThread *newTask(const char *name, F&& func) {
    struct WorkImpl final : threads::WorkThread {
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

    void draw() {
        std::lock_guard lock(mutex);
        for (const auto& message : buffer) {
            ImGui::Text("%s", message.c_str());
        }
    }

    debug::GlobalHandle debug = debug::addGlobalHandle("Logs", [this] { draw(); });

private:
    std::mutex mutex;
    std::vector<std::string> buffer;
};

static GuiLogger *pGuiLogger = nullptr;
static FileLogger *pFileLogger = nullptr;

struct GameWindow final : IWindowCallbacks {
    void onClose() override {
        bWindowOpen = false;
        workPool.clear();
        pWorld->shutdown();
    }

    void onResize(const WindowSize& event) override {
        if (!bWindowOpen) return;
        if (!pWorld) return;

        pWorld->pRenderThread->add("resize-display", [&, w = event.width, h = event.height] {
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

    GameGui(Graph *pGraph, ResourceWrapper<IRTVHandle> *pRenderTarget, ResourceWrapper<ISRVHandle> *pSceneSource)
        : IGuiPass(pGraph, pRenderTarget)
        , pSceneSource(addAttachment(pSceneSource, rhi::ResourceState::eTextureRead))
    {
        pTextHandle = pGraph->addResource<graph::TextHandle>("SwarmFace-Regular");
        pTextAttachment = addAttachment(pTextHandle, rhi::ResourceState::eTextureRead);

        ImPlot::CreateContext();
    }

    ~GameGui() {
        ImPlot::DestroyContext();
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

    debug::GlobalHandle sceneHandle = debug::addGlobalHandle("Scene", [this] { sceneDebug(); });

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

    bool bShowImGuiDemo = false;
    bool bShowImPlotDemo = false;

    void content() override {
        drawDockSpace();

        if (bShowImGuiDemo) ImGui::ShowDemoWindow(&bShowImGuiDemo);
        if (bShowImPlotDemo) ImPlot::ShowDemoWindow(&bShowImPlotDemo);

        debug::enumGlobalHandles([](auto *pHandle) {
            bool bEnabled = pHandle->isEnabled();
            if (!bEnabled) return;

            if (ImGui::Begin(pHandle->getName(), &bEnabled)) {
                pHandle->draw();
            }
            ImGui::End();

            pHandle->setEnabled(bEnabled);
        });

        drawRenderSettings();
        drawFilePicker();
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

    void drawDockSpace() {
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

            if (ImGui::BeginMenu("Windows")) {
                debug::enumGlobalHandles([](auto *pHandle) {
                    bool bEnabled = pHandle->isEnabled();
                    if (ImGui::MenuItem(pHandle->getName(), nullptr, &bEnabled)) {
                        pHandle->setEnabled(bEnabled);
                    }
                });

                ImGui::Separator();
                ImGui::MenuItem("Dear ImGui Demo", nullptr, &bShowImGuiDemo);
                ImGui::MenuItem("ImPlot Demo", nullptr, &bShowImPlotDemo);
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        ImGui::End();
    }

    void drawFilePicker() {
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

    static void drawHeapSlots(bool& open, const char *name, const simcoe::BitMap& alloc) {
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
    static void drawGraphObjects(bool& open, const char *name, const std::vector<T>& objects, F&& drawObject) {
        if (open) {
            ImGui::SetNextItemOpen(true);
        }

        if (ImGui::CollapsingHeader(name)) {
            open = true;
            for (auto& object : objects) {
                drawObject(object);
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

    void drawRenderSettings() {
        if (ImGui::Begin("Render settings")) {
            const auto& createInfo = ctx->getCreateInfo();
            ImGui::Text("Display resolution: %dx%d", createInfo.displayWidth, createInfo.displayHeight);
            ImGui::Text("Internal resolution: %dx%d", createInfo.renderWidth, createInfo.renderHeight);

            int currentWindowMode = gWindowMode;
            if (ImGui::Combo("Window mode", &currentWindowMode, kWindowModeNames.data(), kWindowModeNames.size())) {
                pWorld->pRenderThread->add("change-window-mode", [oldMode = gWindowMode, newMode = currentWindowMode] {
                    changeWindowMode(WindowMode(oldMode), WindowMode(newMode));
                });
            }

            bool bTearing = ctx->bAllowTearing;
            ImGui::Checkbox("Allow tearing", &bTearing);
            ctx->bAllowTearing = bTearing;

            ImGui::Text("DXGI reported fullscreen: %s", ctx->bReportedFullscreen ? "true" : "false");

            if (ImGui::SliderInt2("Internal resolution", renderSize, 64, 4096)) {
                pWorld->pRenderThread->add("resize-render", [this] {
                    pGraph->resizeRender(renderSize[0], renderSize[1]);
                    LOG_INFO("resize-render: {}x{}", renderSize[0], renderSize[1]);
                });
            }

            if (ImGui::SliderInt("backbuffer count", &backBufferCount, 2, 8)) {
                pWorld->pRenderThread->add("change-backbuffers", [this] {
                    pGraph->changeBackBufferCount(backBufferCount);
                    LOG_INFO("change-backbuffer-count: {}", backBufferCount);
                });
            }

            if (ImGui::Combo("Adapter", &currentAdapter, adapterNames.data(), adapterNames.size())) {
                pWorld->pRenderThread->add("change-adapter", [this] {
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

            drawHeapSlots(bRtvOpen, std::format("RTV heap {}", rtvAlloc.getSize()).c_str(), rtvAlloc);
            drawHeapSlots(bDsvOpen, std::format("DSV heap {}", dsvAlloc.getSize()).c_str(), dsvAlloc);
            drawHeapSlots(bSrvOpen, std::format("SRV heap {}", srvAlloc.getSize()).c_str(), srvAlloc);

            ImGui::SeparatorText("RenderGraph state");
            const auto& resources = pGraph->resources;
            const auto& passes = pGraph->passes;
            const auto& objects = pGraph->objects;

            drawGraphObjects(bResourcesOpen, std::format("resources: {}", resources.size()).c_str(), resources, [](sr::IResourceHandle *pResource) {
                auto name = pResource->getName();
                ImGui::Text("%s (state: %s)", name.data(), rhi::toString(pResource->getCurrentState()).data());
            });

            drawGraphObjects(bPassesOpen, std::format("passes: {}", passes.size()).c_str(), passes, [](sr::ICommandPass *pPass) {
                auto name = pPass->getName();
                ImGui::Text("pass: %s", name.data());
                for (auto& resource : pPass->inputs) {
                    auto handle = resource->getResourceHandle();
                    auto state = resource->getRequiredState();
                    ImGui::BulletText("resource: %s (expected: %s)", handle->getName().data(), rhi::toString(state).data());
                }
            });

            drawGraphObjects(bObjectsOpen, std::format("objects: {}", objects.size()).c_str(), objects, [](sr::IGraphObject *pObject) {
                auto name = pObject->getName();
                ImGui::Text("%s", name.data());
            });
        }

        ImGui::End();
    }
};

static GameWindow gWindowCallbacks;

static debug::GlobalHandle startServiceDebugger() {
    debug::GdkDebug *pGdkDebug = new debug::GdkDebug();
    debug::RyzenMonitorDebug *pRyzenDebug = new debug::RyzenMonitorDebug();
    debug::EngineDebug *pEngineDebug = new debug::EngineDebug(pWorld);

    const auto services = std::to_array({
        static_cast<debug::ServiceDebug*>(pGdkDebug),
        static_cast<debug::ServiceDebug*>(pRyzenDebug),
        static_cast<debug::ServiceDebug*>(pEngineDebug)
    });

    if (RyzenMonitorSerivce::getState() & eServiceCreated) {
        workPool.emplace_back(pRyzenDebug->getWorkThread());
    }

    return debug::addGlobalHandle("Services", [=] {
        if (ImGui::BeginTabBar("ServiceTabs")) {
            for (auto *pHandle : services) {
                auto error = pHandle->getFailureReason();
                auto name = pHandle->getName();

                ImGui::BeginDisabled(!error.empty());

                if (ImGui::BeginTabItem(name.data())) {
                    pHandle->draw();
                    ImGui::EndTabItem();
                }

                ImGui::EndDisabled();

                if (!error.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
                    ImGui::SetTooltip("%s", error.data());
                }
            }
            ImGui::EndTabBar();
        }
    });
}

///
/// entry point
///

static void commonMain() {
    pMainQueue = new threads::WorkQueue(64);

    auto assets = PlatformService::getExeDirectory() / "editor.exe.p";
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
    // pInput->addClient(swarm::getInputClient());

    const sr::RenderCreateInfo renderCreateInfo = {
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
    pContext = sr::Context::create(renderCreateInfo);
    pGraph = new Graph(pContext);

    // setup render

    auto *pBackBuffers = pGraph->addResource<graph::SwapChainHandle>();
    auto *pSceneTarget = pGraph->addResource<graph::SceneTargetHandle>();
    auto *pDepthTarget = pGraph->addResource<graph::DepthTargetHandle>();

    pGraph->addPass<graph::ScenePass>(pSceneTarget->as<IRTVHandle>());

    // pGraph->addPass<graph::GameLevelPass>(pSceneTarget->as<IRTVHandle>(), pDepthTarget->as<IDSVHandle>());

    gr::ScenePass *pScenePass = pGraph->addPass<gr::ScenePass>(pSceneTarget->as<IRTVHandle>(), pDepthTarget->as<IDSVHandle>());
    gr::HudPass *pHudPass = pGraph->addPass<gr::HudPass>(pSceneTarget->as<IRTVHandle>());

    // pGraph->addPass<graph::PostPass>(pBackBuffers->as<IRTVHandle>(), pSceneTarget->as<ISRVHandle>());

    pGraph->addPass<GameGui>(pBackBuffers->as<IRTVHandle>(), pSceneTarget->as<ISRVHandle>());

    pGraph->addPass<graph::PresentPass>(pBackBuffers);

    const game::WorldInfo worldInfo = {
        // game config
        .entityLimit = 0x1000,
        .seed = 0,

        // input config
        .pInput = pInput,

        // render config
        .pRenderContext = pContext,
        .pRenderGraph = pGraph,
        .renderFaultLimit = 3,

        // game render config
        .pHudPass = pHudPass,
        .pScenePass = pScenePass
    };

    pWorld = new game::World(worldInfo);

    auto dbg = startServiceDebugger();

    // setup game

    workPool.emplace_back([](auto token) {
        DebugService::setThreadName("input");

        while (!token.stop_requested()) {
            pWorld->tickInput();
        }
    });

    workPool.emplace_back([](auto token) {
        DebugService::setThreadName("render");

        while (!token.stop_requested() && bWindowOpen) {
            pWorld->tickRender();
        }
    });

    workPool.emplace_back([](auto token) {
        DebugService::setThreadName("physics");

        while (!token.stop_requested()) {
            pWorld->tickPhysics();
        }
    });

    workPool.emplace_back([](auto token) {
        DebugService::setThreadName("game");

        while (!token.stop_requested()) {
            pWorld->tickGame();
        }
    });

    while (PlatformService::waitForEvent() && !pWorld->shouldQuit()) {
        PlatformService::dispatchEvent();

        pMainQueue->process();
    }

    PlatformService::quit();
}

static int serviceWrapper() try {
    DebugService::setThreadName("main");

    pFileLogger = LoggingService::newSink<FileLogger>();
    pGuiLogger = LoggingService::newSink<GuiLogger>();

    auto engineServices = std::to_array({
        DebugService::service(),

        LoggingService::service(),
        PlatformService::service(),
        ThreadService::service(),
        FreeTypeService::service(),

        GdkService::service(),
        RyzenMonitorSerivce::service()
    });
    ServiceRuntime runtime{engineServices};

    // dont use a Region here because we dont want to print `shutdown` if an exception is thrown
    commonMain();
    LOG_INFO("no game exceptions have occured during runtime");

    return 0;
} catch (const std::exception& err) {
    LOG_ERROR("unhandled exception: {}", err.what());
    return 99;
} catch (...) {
    LOG_ERROR("unhandled exception");
    return 99;
}

static int innerMain() {
    LOG_INFO("bringing up services");
    int result = serviceWrapper();
    LOG_INFO("all services shut down gracefully");
    return result;
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
