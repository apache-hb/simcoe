#include "editor/service.h"

#include "editor/ui/panels/audio.h"
#include "engine/render/graph.h"
#include "engine/config/system.h"
#include "engine/rhi/service.h"

// editor ui
#include "editor/ui/panels/config.h"
#include "editor/ui/panels/depot.h"
#include "editor/ui/panels/world.h"
#include "editor/ui/panels/threads.h"
#include "editor/ui/panels/ryzenmonitor.h"
#include "editor/ui/panels/gameruntime.h"

// render passes
#include "editor/graph/assets.h"
#include "editor/graph/post.h"
#include "editor/graph/scene.h"
#include "editor/graph/gui.h"

// game
#include "game/render/hud.h"
#include "game/render/scene.h"

// imgui
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include "imfiles/imfilebrowser.h"
#include "implot/implot.h"


using namespace game;
using namespace editor;

namespace eg = editor::graph;
namespace sr = simcoe::render;

// window mode
static constexpr auto kWindowModeNames = std::to_array({ "Windowed", "Borderless", "Fullscreen" });

namespace {
    // render
    WindowMode windowMode = eModeWindowed;

    std::vector<editor::ui::ServiceUi*> debugServices;
}

config::ConfigValue<size_t> cfgEntityLimit("game", "entity_limit", "upper entity limit", 0x1000);
config::ConfigValue<size_t> cfgSeed("game", "seed", "World seed", 0);

void setWindowMode(WindowMode oldMode, WindowMode newMode) {
    if (oldMode == newMode) return;
    windowMode = newMode;

    auto& window = PlatformService::getWindow();

    if (oldMode == eModeFullscreen) {
        RenderService::setFullscreen(false);
        window.exitFullscreen();
        return;
    }

    switch (newMode) {
    case eModeWindowed:
        window.setStyle(WindowStyle::eWindowed);
        break;
    case eModeBorderless:
        window.setStyle(WindowStyle::eBorderlessFixed);
        break;
    case eModeFullscreen:
        RenderService::setFullscreen(true);
        window.enterFullscreen();
        break;

    default:
        break;
    }
}

struct EditorUi final : eg::IGuiPass {
    using eg::IGuiPass::IGuiPass;

    int renderSize[2];
    int backBufferCount;

    int currentAdapter = 0;
    std::vector<const char*> adapterNames;

    ImGui::FileBrowser imgLoadBrowser;
    ImGui::FileBrowser objFileBrowser;
    ImGui::FileBrowser imguiFileBrowser{ImGuiFileBrowserFlags_EnterNewFilename};

    PassAttachment<ISRVHandle> *pSceneSource = nullptr;

    ResourceWrapper<eg::TextHandle> *pTextHandle = nullptr;
    PassAttachment<eg::TextHandle> *pTextAttachment = nullptr;

    struct ImageData {
        std::string name;
        ResourceWrapper<eg::TextureHandle> *pHandle = nullptr;
        PassAttachment<eg::TextureHandle> *pAttachment = nullptr;
    };

    mt::SharedMutex imageLock{"GameGui::imageLock"};
    std::vector<ImageData> images;
    int currentImage = 0;

    void addImage(std::string imageName) try {
        auto *pHandle = pGraph->addResource<eg::TextureHandle>(imageName);
        auto *pAttachment = addAttachment(pHandle, rhi::ResourceState::eTextureRead);

        // lock the image list AFTER we've created the image
        // to prevent a deadlock between the render thread and the worker thread
        // loading the image
        mt::WriteLock lock(imageLock);
        images.push_back({ imageName, pHandle, pAttachment });
        currentImage = core::intCast<int>(images.size()) - 1;
    } catch (const core::Error& err) {
        LOG_WARN("failed to load image: {}", err.what());
    }

    ui::GlobalHandle imageHandle = ui::addGlobalHandle("Images", [this] {
        // draw a grid of images
        float windowWidth = ImGui::GetWindowWidth();
        float cellWidth = 250.f;
        size_t cols = std::clamp<size_t>(size_t(windowWidth / cellWidth), 1, 8);

        mt::ReadLock lock(imageLock);
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

            const auto& [imageName, pImageHandle, pAttach] = images[i];
            ImGuiSelectableFlags flags = ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_AllowOverlap;
            ImVec2 size = { cellWidth, cellWidth };
            auto offset = ctx->getSrvHeap()->deviceOffset(pImageHandle->getInner()->getSrvIndex());

            ImVec2 before = ImGui::GetCursorPos();
            ImGui::Image((ImTextureID)offset, { cellWidth, cellWidth });
            ImGui::SetCursorPos(before);

            bool bSelectedImage = int(i) == currentImage;
            if (bSelectedImage) ImGui::PushStyleColor(ImGuiCol_Header, color);

            if (ImGui::Selectable(imageName.c_str(), bSelectedImage, flags, size)) {
                currentImage = core::intCast<int>(i);
            }

            if (bSelectedImage) ImGui::PopStyleColor();

            ImGui::PopID();
        }
        ImGui::PopStyleColor();
    });

    EditorUi(Graph *pGraph, ResourceWrapper<IRTVHandle> *pRenderTarget, ResourceWrapper<ISRVHandle> *pSceneSource)
        : IGuiPass(pGraph, pRenderTarget)
        , pSceneSource(addAttachment(pSceneSource, rhi::ResourceState::eTextureRead))
    {
        pTextHandle = pGraph->addResource<eg::TextHandle>("SwarmFace-Regular");
        pTextAttachment = addAttachment(pTextHandle, rhi::ResourceState::eTextureRead);

        addImage("meme.jpg");

        ImPlot::CreateContext();
    }

    ~EditorUi() {
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

    ui::GlobalHandle sceneHandle = ui::addGlobalHandle("Scene", [this] { sceneDebug(); });

    void create() override {
        IGuiPass::create();

        const auto &createInfo = ctx->getCreateInfo();

        renderSize[0] = core::intCast<int>(createInfo.renderWidth);
        renderSize[1] = core::intCast<int>(createInfo.renderHeight);

        backBufferCount = core::intCast<int>(createInfo.backBufferCount);
        currentAdapter = INT_MAX;

        auto *pCurrentAdapter = GpuService::getSelectedAdapter();
        auto currentInfo = pCurrentAdapter->getInfo();

        auto adapterSpan = GpuService::getAdapters();

        for (size_t i = 0; i < adapterSpan.size(); i++) {
            auto *pAdapter = adapterSpan[i];
            auto info = pAdapter->getInfo();

            // TODO: this is almost definetly close enough, im just paranoid
            if (currentInfo.vendorId == info.vendorId && currentInfo.deviceId == info.deviceId) {
                currentAdapter = core::intCast<int>(i);
            }

            adapterNames.push_back(_strdup(info.name.c_str()));
        }

        SM_ASSERTF(currentAdapter != INT_MAX, "could not find current adapter {} in adapter list", pCurrentAdapter->getInfo().name);
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

        ui::enumGlobalHandles([](auto *pHandle) {
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

        for (auto *pService : EditorService::getDebugServices()) {
            pService->drawWindow();
        }
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
                ui::enumGlobalHandles([](auto *pHandle) {
                    bool bEnabled = pHandle->isEnabled();
                    if (ImGui::MenuItem(pHandle->getName(), nullptr, &bEnabled)) {
                        pHandle->setEnabled(bEnabled);
                    }
                });

                ImGui::SeparatorText("Services");
                for (auto *pService : EditorService::getDebugServices()) {
                    pService->drawMenuItem();
                }

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

            ThreadService::enqueueWork("add-image", [this, path] {
                addImage(path);
            });
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

            int currentWindowMode = EditorService::getWindowMode();
            if (ImGui::Combo("Window mode", &currentWindowMode, kWindowModeNames.data(), core::intCast<int>(kWindowModeNames.size()))) {
                EditorService::changeWindowMode(WindowMode(currentWindowMode));
            }

            bool bTearing = ctx->bAllowTearing;
            ImGui::Checkbox("Allow tearing", &bTearing);
            ctx->bAllowTearing = bTearing;

            ImGui::Text("DXGI reported fullscreen: %s", ctx->bReportedFullscreen ? "true" : "false");

            if (ImGui::SliderInt2("Internal resolution", renderSize, 64, 4096)) {
                EditorService::changeInternalRes({ uint32_t(renderSize[0]), uint32_t(renderSize[1]) });
            }

            if (ImGui::SliderInt("backbuffer count", &backBufferCount, 2, 8)) {
                EditorService::changeBackBufferCount(uint32_t(backBufferCount));
            }

            if (ImGui::Combo("Adapter", &currentAdapter, adapterNames.data(), core::intCast<int>(adapterNames.size()))) {
                EditorService::changeCurrentAdapter(uint32_t(currentAdapter));
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

bool EditorService::createService() {
    auto *pGraph = RenderService::getGraph();
    
    auto *pBackBuffers = pGraph->addResource<eg::SwapChainHandle>();
    auto *pSceneTarget = pGraph->addResource<eg::SceneTargetHandle>();
    // auto *pDepthTarget = pGraph->addResource<eg::DepthTargetHandle>();

    //pGraph->addPass<graph::ScenePass>(pSceneTarget->as<IRTVHandle>(), pDepthTarget->as<IDSVHandle>());

    // pGraph->addPass<graph::GameLevelPass>(pSceneTarget->as<IRTVHandle>(), pDepthTarget->as<IDSVHandle>());

    // gr::ScenePass *pScenePass = pGraph->addPass<gr::ScenePass>(pSceneTarget->as<IRTVHandle>(), pDepthTarget->as<IDSVHandle>());
    // gr::HudPass *pHudPass = pGraph->addPass<gr::HudPass>(pSceneTarget->as<IRTVHandle>());

    // pGraph->addPass<graph::PostPass>(pBackBuffers->as<IRTVHandle>(), pSceneTarget->as<ISRVHandle>());

    pGraph->addPass<EditorUi>(pBackBuffers->as<IRTVHandle>(), pSceneTarget->as<ISRVHandle>());

    pGraph->addPass<eg::PresentPass>(pBackBuffers);

    return true;
}

void EditorService::destroyService() {

}

void EditorService::start() {
    addDebugService<ui::ConfigUi>();
    addDebugService<ui::DepotUi>();
    // addDebugService<ui::WorldUi>(pWorld);
    addDebugService<ui::AudioUi>();
    addDebugService<ui::GameRuntimeUi>();
    addDebugService<ui::ThreadServiceUi>();
    addDebugService<ui::RyzenMonitorUi>();
}

void EditorService::resizeDisplay(const WindowSize& event) {
    RenderService::resizeDisplay(event);
}

// editable stuff
WindowMode EditorService::getWindowMode() {
    return windowMode;
}

void EditorService::changeWindowMode(WindowMode newMode) {
    RenderService::enqueueWork("modechange", [newMode]() {
        setWindowMode(getWindowMode(), newMode);
    });
}

void EditorService::changeInternalRes(const simcoe::math::uint2& newRes) {
    RenderService::resizeRender(newRes);
}

void EditorService::changeBackBufferCount(UINT newCount) {
    RenderService::changeBackBufferCount(newCount);
}

void EditorService::changeCurrentAdapter(UINT newAdapter) {
    RenderService::changeAdapter(newAdapter);
}

void EditorService::addDebugService(editor::ui::ServiceUi *pService) {
    debugServices.push_back(pService);
}

std::span<editor::ui::ServiceUi*> EditorService::getDebugServices() {
    return debugServices;
}
