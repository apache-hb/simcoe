// core
#include "engine/core/mt.h"

// services
#include "engine/service/service.h"
#include "engine/service/debug.h"
#include "engine/log/service.h"
#include "engine/service/platform.h"
#include "engine/service/freetype.h"

// threads
#include "engine/threads/schedule.h"
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
#include "editor/debug/logging.h"

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

using namespace std::chrono_literals;

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
static game::World *pWorld = nullptr;

// window mode
static Window *pWindow = nullptr;
static std::atomic_bool bWindowOpen = true;
static WindowMode gWindowMode = eModeWindowed;
static constexpr auto kWindowModeNames = std::to_array({ "Windowed", "Borderless", "Fullscreen" });

/// threads
static threads::WorkQueue *pMainQueue = nullptr;

/// input
static input::Win32Keyboard *pKeyboard = nullptr;
static input::Win32Mouse *pMouse = nullptr;
static input::XInputGamepad *pGamepad0 = nullptr;
static input::Manager *pInput = nullptr;

/// rendering
static sr::Context *pContext = nullptr;
static sr::Graph *pGraph = nullptr;

// debuggers
static debug::LoggingDebug *pLoggingDebug = new debug::LoggingDebug();
static debug::GdkDebug *pGdkDebug = nullptr;
static debug::RyzenMonitorDebug *pRyzenDebug = nullptr;
static debug::EngineDebug *pEngineDebug = nullptr;
static debug::ThreadServiceDebug *pThreadDebug = nullptr;

struct GameWindow final : IWindowCallbacks {
    void onClose() override {
        bWindowOpen = false;
        ThreadService::shutdown();
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

    ImGui::FileBrowser imgLoadBrowser;
    ImGui::FileBrowser objFileBrowser;
    ImGui::FileBrowser imguiFileBrowser{ImGuiFileBrowserFlags_EnterNewFilename};

    PassAttachment<ISRVHandle> *pSceneSource = nullptr;

    ResourceWrapper<graph::TextHandle> *pTextHandle = nullptr;
    PassAttachment<graph::TextHandle> *pTextAttachment = nullptr;

    struct ImageData {
        std::string name;
        ResourceWrapper<graph::TextureHandle> *pHandle = nullptr;
        PassAttachment<graph::TextureHandle> *pAttachment = nullptr;
    };

    std::vector<ImageData> images;
    int currentImage = 0;

    void addImage(std::string imageName) {
        auto *pHandle = pGraph->addResource<graph::TextureHandle>(imageName);
        auto *pAttachment = addAttachment(pHandle, rhi::ResourceState::eTextureRead);

        images.push_back({ imageName, pHandle, pAttachment });
        currentImage = core::intCast<int>(images.size()) - 1;
    }

    debug::GlobalHandle imageHandle = debug::addGlobalHandle("Images", [this] {
        // draw a grid of images
        float windowWidth = ImGui::GetWindowWidth();
        float cellWidth = 250.f;
        size_t cols = std::clamp<size_t>(size_t(windowWidth / cellWidth), 1, 8);

        if (ImGui::BeginCombo("Image", images[currentImage].name.c_str())) {
            for (size_t i = 0; i < images.size(); i++) {
                bool bSelected = int(i) == currentImage;
                if (ImGui::Selectable(images[i].name.c_str(), bSelected)) {
                    currentImage = core::intCast<int>(i);
                }
            }
            ImGui::EndCombo();
        }

        ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Header);
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
        for (size_t i = 0; i < images.size(); i++) {
            if (i % cols != 0) ImGui::SameLine();
            ImGui::PushID(int(i));

            auto& image = images[i];
            ImGuiSelectableFlags flags = ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_AllowOverlap;
            ImVec2 size = { cellWidth, cellWidth };
            auto *pHandle = image.pHandle;
            auto offset = ctx->getSrvHeap()->deviceOffset(pHandle->getInner()->getSrvIndex());

            ImVec2 before = ImGui::GetCursorPos();
            ImGui::Image((ImTextureID)offset, { cellWidth, cellWidth });
            ImGui::SetCursorPos(before);

            bool bSelectedImage = int(i) == currentImage;
            if (bSelectedImage) ImGui::PushStyleColor(ImGuiCol_Header, color);

            if (ImGui::Selectable(image.name.c_str(), bSelectedImage, flags, size)) {
                currentImage = core::intCast<int>(i);
            }

            if (bSelectedImage) ImGui::PopStyleColor();

            ImGui::PopID();
        }
        ImGui::PopStyleColor();
    });

    GameGui(Graph *pGraph, ResourceWrapper<IRTVHandle> *pRenderTarget, ResourceWrapper<ISRVHandle> *pSceneSource)
        : IGuiPass(pGraph, pRenderTarget)
        , pSceneSource(addAttachment(pSceneSource, rhi::ResourceState::eTextureRead))
    {
        pTextHandle = pGraph->addResource<graph::TextHandle>("SwarmFace-Regular");
        pTextAttachment = addAttachment(pTextHandle, rhi::ResourceState::eTextureRead);

        addImage("meme.jpg");

        ImPlot::CreateContext();
    }

    ~GameGui() {
        ImPlot::DestroyContext();
    }

    void sceneDebug() {
        auto *pImageHandle = images[currentImage].pHandle;
        ISRVHandle *pHandle = pImageHandle->getInner(); //pSceneSource->getInner();
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

        const auto &createInfo = ctx->getCreateInfo();

        renderSize[0] = core::intCast<int>(createInfo.renderWidth);
        renderSize[1] = core::intCast<int>(createInfo.renderHeight);

        backBufferCount = core::intCast<int>(createInfo.backBufferCount);
        currentAdapter = core::intCast<int>(createInfo.adapterIndex);

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

        pLoggingDebug->drawWindow();
        pGdkDebug->drawWindow();
        pRyzenDebug->drawWindow();
        pEngineDebug->drawWindow();
        pThreadDebug->drawWindow();
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

                if (ImGui::MenuItem("Import Model")) {
                    objFileBrowser.SetTitle("Open OBJ File");
                    objFileBrowser.SetTypeFilters({ ".obj" });
                    objFileBrowser.Open();
                }

                if (ImGui::MenuItem("Import Image")) {
                    imgLoadBrowser.SetTitle("Open Image File");
                    imgLoadBrowser.SetTypeFilters({ ".jpg", ".png" });
                    imgLoadBrowser.Open();
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

                ImGui::SeparatorText("Services");
                pLoggingDebug->drawMenuItem();
                pGdkDebug->drawMenuItem();
                pRyzenDebug->drawMenuItem();
                pEngineDebug->drawMenuItem();
                pThreadDebug->drawMenuItem();

                ImGui::SeparatorText("ImGui");
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
        imgLoadBrowser.Display();

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

        if (imgLoadBrowser.HasSelected()) {
            auto path = imgLoadBrowser.GetSelected().string();
            LOG_INFO("selected: {}", path);
            imgLoadBrowser.ClearSelected();

            addImage(path);
        }
    }

    static void drawHeapSlots(bool& open, const char *name, const core::BitMap& alloc) {
        if (open) {
            ImGui::SetNextItemOpen(true);
        }

        if (ImGui::CollapsingHeader(name)) {
            open = true;
            // show a grid of slots
            size_t size = alloc.getSize();
            size_t rows = size / 8;
            size_t cols = size / rows;

            ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchSame
                                  | ImGuiTableFlags_BordersInner
                                  | ImGuiTableFlags_RowBg;

            if (ImGui::BeginTable("Slots", core::intCast<int>(cols), flags)) {
                for (size_t i = 0; i < size; i++) {
                    ImGui::TableNextColumn();
                    if (alloc.test(core::BitMap::Index(i))) {
                        ImGui::Text("%zu (used)", i);
                    } else {
                        ImGui::TextDisabled("%zu (free)", i);
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
            if (ImGui::Combo("Window mode", &currentWindowMode, kWindowModeNames.data(), core::intCast<int>(kWindowModeNames.size()))) {
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

            if (ImGui::Combo("Adapter", &currentAdapter, adapterNames.data(), core::intCast<int>(adapterNames.size()))) {
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

static void startServiceDebuggers() {
    pGdkDebug = new debug::GdkDebug();
    pRyzenDebug = new debug::RyzenMonitorDebug();
    pEngineDebug = new debug::EngineDebug(pWorld);
    pThreadDebug = new debug::ThreadServiceDebug();

    if (RyzenMonitorSerivce::getState() & eServiceCreated) {
        ThreadService::newJob("ryzenmonitor", 1s, [] {
            pRyzenDebug->updateCoreInfo();
        });
    }
}

///
/// entry point
///

static void commonMain() {
    ThreadService::setThreadName("main");

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

    startServiceDebuggers();

    // setup game

    ThreadService::newThread(threads::eResponsive, "input", [](auto token) {
        while (!token.stop_requested()) {
            pWorld->tickInput();
        }
    });

    ThreadService::newThread(threads::eRealtime, "render", [](auto token) {
        while (!token.stop_requested() && bWindowOpen) {
            pWorld->tickRender();
        }
    });

    ThreadService::newThread(threads::eResponsive, "physics", [](auto token) {
        while (!token.stop_requested()) {
            pWorld->tickPhysics();
        }
    });

    ThreadService::newThread(threads::eRealtime, "game", [](auto token) {
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
    std::ofstream fd("editor.log");
    log::StreamSink fdSink(fd);
    LoggingService::addSink(pLoggingDebug);
    LoggingService::addSink(&fdSink);

    auto engineServices = std::to_array({
        DebugService::service(),

        PlatformService::service(),
        LoggingService::service(),
        ThreadService::service(),
        FreeTypeService::service(),

        GdkService::service(),
        RyzenMonitorSerivce::service()
    });
    ServiceRuntime runtime{engineServices, "editor"};

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

int wWinMain(HINSTANCE hInstance, SM_UNUSED HINSTANCE hPrevInstance, SM_UNUSED LPWSTR lpCmdLine, int nCmdShow) {
    PlatformService::setup(hInstance, nCmdShow);
    return innerMain();
}

// command line entry point

int main(SM_UNUSED int argc, SM_UNUSED const char **argv) {
    PlatformService::setup(GetModuleHandle(nullptr), SW_SHOWDEFAULT);
    return innerMain();
}
