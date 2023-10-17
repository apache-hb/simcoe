// os and system
#include "engine/engine.h"
#include "engine/os/system.h"

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
#include "editor/debug/gui.h"
#include "imgui/imgui.h"
#include "imfiles/imfilebrowser.h"

// vendor
#include "vendor/microsoft/gdk.h"
#include "moodycamel/concurrentqueue.h"

#include <functional>
#include <array>
#include <fstream>

using namespace simcoe;
using namespace simcoe::math;

using namespace editor;

namespace gdk = microsoft::gdk;
namespace fs = std::filesystem;

// consts
static constexpr auto kWindowWidth = 1920;
static constexpr auto kWindowHeight = 1080;

static constexpr auto kProjectionNames = std::to_array({ "Perspective", "Orthographic" });
static const auto gProjections = std::to_array({
    static_cast<IProjection*>(new Perspective()),
    static_cast<IProjection*>(new Orthographic(24.f, 24.f))
});

// system
static simcoe::System *pSystem = nullptr;
static simcoe::Window *pWindow = nullptr;
static bool gFullscreen = false;
static std::atomic_bool gRunning = true;
std::string gGdkFailureReason = "";

/// threads
static tasks::WorkQueue *pMainQueue = nullptr;
static tasks::WorkThread *pWorkThread = nullptr;
static tasks::WorkThread *pRenderThread = nullptr;
static tasks::WorkThread *pGameThread = nullptr;

/// input

static input::Win32Keyboard *pKeyboard = nullptr;
static input::Win32Mouse *pMouse = nullptr;
static input::XInputGamepad *pGamepad0 = nullptr;
static input::Manager *pInput = nullptr;

/// rendering
static render::Graph *pGraph = nullptr;
static int gCurrentProjection = 1;

static IMeshBufferHandle *pPlayerMesh = nullptr;
static IMeshBufferHandle *pGridMesh = nullptr;
static IMeshBufferHandle *pAlienMesh = nullptr;

static IMeshBufferHandle *pBulletMesh = nullptr;
static IMeshBufferHandle *pEggSmallMesh = nullptr;
static IMeshBufferHandle *pEggMediumMesh = nullptr;
static IMeshBufferHandle *pEggLargeMesh = nullptr;

static size_t gPlayerTextureId = SIZE_MAX;
static size_t gCrossTextureId = SIZE_MAX;
static size_t gAlienTextureId = SIZE_MAX;

static size_t gBulletTextureId = SIZE_MAX;

static size_t gEggSmallTextureId = SIZE_MAX;
static size_t gEggMediumTextureId = SIZE_MAX;
static size_t gEggLargeTextureId = SIZE_MAX;

/// game level

GameLevel gLevel;

struct PlayerObject : IGameObject {
    PlayerObject(GameLevel *pLevel, std::string name) : IGameObject(pLevel, name) {
        setMesh(pPlayerMesh);
        setTextureId(gPlayerTextureId);
    }
};

struct AlienObject : IGameObject {
    AlienObject(GameLevel *pLevel, std::string name) : IGameObject(pLevel, name) {
        setMesh(pAlienMesh);
        setTextureId(gAlienTextureId);
    }
};

struct BulletObject : IGameObject {
    BulletObject(GameLevel *pLevel, std::string name, float2 velocity)
        : IGameObject(pLevel, name)
        , velocity(velocity)
    {
        setMesh(pBulletMesh);
        setTextureId(gBulletTextureId);
    }

    void tick(float delta) override {
        position += float3::from(0.f, velocity * delta);
    }
private:
    float2 velocity;
};

struct EggObject : IGameObject {
    EggObject(GameLevel *pLevel, std::string name) : IGameObject(pLevel, name) {
        setMesh(pEggSmallMesh);
        setTextureId(gEggSmallTextureId);
    }

    void tick(float delta) {
        timeAlive += delta;

        if (timeAlive > kTimeToHatch) {
            pLevel->deleteObject(this);
        } else if (timeAlive > kTimeToLarge) {
            setMesh(pEggLargeMesh);
            setTextureId(gEggLargeTextureId);
        } else if (timeAlive > kTimeToMedium) {
            setMesh(pEggMediumMesh);
            setTextureId(gEggMediumTextureId);
        }
    }

private:
    static constexpr auto kTimeToMedium = 1.5f;
    static constexpr auto kTimeToLarge = 3.f;
    static constexpr auto kTimeToHatch = 5.f;

    float timeAlive = 0.f;
};

struct CrossObject : IGameObject {
    CrossObject(GameLevel *pLevel, std::string name) : IGameObject(pLevel, name) {
        setMesh(pGridMesh);
        setTextureId(gCrossTextureId);
    }
};

static PlayerObject *pPlayerObject = nullptr;
static AlienObject *pEnemyObject = nullptr;

static void createAlien(std::string name) {
    pEnemyObject = gLevel.addObject<AlienObject>(name);
}

static void createPlayer(std::string name) {
    pPlayerObject = gLevel.addObject<PlayerObject>(name);
}

static CrossObject *addCross(std::string name) {
    return gLevel.addObject<CrossObject>(name);
}

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
        pSystem->quit();
    }

    void onResize(const ResizeEvent& event) override {
        pWorkThread->add("resize-display", [&, event] {
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

using namespace simcoe::input;

struct GameInputClient final : input::IClient {
    Event shootKeyEvent;
    Event shootGamepadEvent;

    float getButtonAxis(Button neg, Button pos) const {
        size_t negIdx = state.buttons[neg];
        size_t posIdx = state.buttons[pos];

        if (negIdx > posIdx) {
            return -1.f;
        } else if (posIdx > negIdx) {
            return 1.f;
        } else {
            return 0.f;
        }
    }

    float getStickAxis(Axis axis) const {
        return state.axes[axis];
    }

    void onInput(const input::State& newState) override {
        state = newState;
        updates += 1;

        shootKeyEvent.update(state.buttons[Button::eKeySpace]);
        shootGamepadEvent.update(state.buttons[Button::ePadButtonDown]);
    }

    static constexpr ImGuiTableFlags kTableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersH | ImGuiTableFlags_BordersV;

    void debugDraw() {
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
    std::atomic_size_t updates = 0;

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

            float avail = ImGui::GetWindowWidth();

            ImGui::Image((ImTextureID)offset, { avail, avail / aspect });
        }

        ImGui::End();

        ImGui::Begin("Game Objects");

        gLevel.useEachObject([&](IGameObject *pObject) {
            ImGui::PushID((void*)pObject);

            auto name = pObject->getName();
            auto model = pObject->getMesh();

            ImGui::BulletText("%s", name.data());
            ImGui::SameLine();
            if (ImGui::Button("Delete")) {
                gLevel.removeObject(pObject);
            } else {
                auto modelName = model->getName();
                ImGui::Text("Mesh: %s", modelName.data());
                ImGui::SliderFloat3("position", pObject->position.data(), -20.f, 20.f);
                ImGui::SliderFloat3("rotation", pObject->rotation.data(), -1.f, 1.f);
                ImGui::SliderFloat3("scale", pObject->scale.data(), 0.1f, 10.f);
            }

            ImGui::PopID();
        });

        ImGui::SeparatorText("Add Object");

        ImGui::InputText("name", objectName, std::size(objectName));

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

            pWorkThread->add("load-obj", [path] {
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
                pRenderThread->add("change-fullscreen", [fs = gFullscreen] {
                    pGraph->setFullscreen(fs);
                    if (fs) {
                        pWindow->enterFullscreen();
                    } else {
                        pWindow->exitFullscreen();
                    }
                });
            }

            bool bTearing = ctx->bAllowTearing;
            ImGui::Checkbox("tearing", &bTearing);
            ctx->bAllowTearing = bTearing;

            ImGui::Text("DXGI reported fullscreen: %s", ctx->bReportedFullscreen ? "true" : "false");

            if (ImGui::SliderInt2("render size", renderSize, 64, 4096)) {
                pRenderThread->add("resize-render", [this] {
                    pGraph->resizeRender(renderSize[0], renderSize[1]);
                    logInfo("resize-render: {}x{}", renderSize[0], renderSize[1]);
                });
            }

            if (ImGui::SliderInt("backbuffer count", &backBufferCount, 2, 8)) {
                pRenderThread->add("change-backbuffers", [this] {
                    pGraph->changeBackBufferCount(backBufferCount);
                    logInfo("change-backbuffer-count: {}", backBufferCount);
                });
            }

            if (ImGui::Combo("device", &currentAdapter, adapterNames.data(), adapterNames.size())) {
                pRenderThread->add("change-adapter", [this] {
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
            ImGui::SliderFloat3("position", gLevel.cameraPosition.data(), -20.f, 20.f);
            ImGui::SliderFloat3("rotation", gLevel.cameraRotation.data(), -1.f, 1.f);

            if (ImGui::Combo("projection", &gCurrentProjection, kProjectionNames.data(), kProjectionNames.size())) {
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

struct GameWorld {
    float3 worldScale = float3::from(1.f, 1.f, 1.f) * float3::from(0.5f);
    float3 worldOrigin = float3::from(0.f, 0.f, 0.f);

    float3 getWorldPos(float x, float y, float index = 0) {
        return worldOrigin + float3::from(index, x - 0.5f, y - 0.5f);
    }

    float3 getWorldScale() const {
        return worldScale;
    }

    float2 getWorldLimits() const {
        return { float(width - 1), float(height) };
    }

    float2 getAlienSpawn() const {
        return { 0.f, float(height - 1) };
    }

    float2 getPlayerSpawn() const {
        return { 0.f, float(height - 2) };
    }

    size_t width = 22;
    size_t height = 19;

    size_t maxLives = 5;
    size_t currentLives = 3;

    std::vector<IGameObject*> pLifeObjects;
};

GameWorld gWorld;

static void createGameThread() {
    pGameThread = newTask("game", [](tasks::WorkQueue *pSelf, auto token) {
        Timer timer;
        float lastTick = timer.now();

        const float playerSpeed = 5.f;
        const float enemySpeed = 2.f;

        const float2 limits = gWorld.getWorldLimits();

        const float fireRate = 0.3f;
        float lastFire = -1.f;

        auto updateEnemy = [&](float delta) {
            pEnemyObject->position.y += enemySpeed * delta;
            if (pEnemyObject->position.y > limits.y) {
                pEnemyObject->position.y = 0.f;
            }
        };

        auto updatePlayer = [&](float delta) {
            float bx = gInputClient.getButtonAxis(Button::eKeyLeft, Button::eKeyRight);
            float by = gInputClient.getButtonAxis(Button::eKeyDown, Button::eKeyUp);

            float kx = gInputClient.getButtonAxis(Button::eKeyA, Button::eKeyD);
            float ky = gInputClient.getButtonAxis(Button::eKeyS, Button::eKeyW);

            float ax = gInputClient.getStickAxis(Axis::eGamepadLeftX);
            float ay = gInputClient.getStickAxis(Axis::eGamepadLeftY);

            float tx = (bx + kx + ax);
            float ty = (by + ky + ay);

            pPlayerObject->position += float3(0.f, tx * playerSpeed * delta, ty * playerSpeed * delta);

            pPlayerObject->position.y = math::clamp(pPlayerObject->position.y, 0.f, limits.x);
            pPlayerObject->position.z = math::clamp(pPlayerObject->position.z, 0.f, limits.y);

            float angle = std::atan2(ty, tx);

            if (tx != 0.f || ty != 0.f)
                pPlayerObject->rotation.x = -angle;

            if (gInputClient.shootKeyEvent.isHeld() || gInputClient.shootGamepadEvent.isHeld()) {
                float now = timer.now();
                if (now - lastFire > fireRate) {
                    lastFire = now;

                    float2 velocity = float2::from(std::cos(angle), std::sin(angle)) * 10.f;

                    auto *pBullet = gLevel.addObject<BulletObject>("bullet", velocity);
                    pBullet->position = pPlayerObject->position;
                    pBullet->rotation = pPlayerObject->rotation;
                    pBullet->scale = gWorld.getWorldScale() * 0.3f;
                }
            }
        };

        auto isObjectInBounds = [&](IGameObject *pObject) {
            float2 pos = pObject->position.yz();
            return pos.x >= 0.f && pos.x < limits.x && pos.y >= 0.f && pos.y < limits.y;
        };

        while (!token.stop_requested()) {
            float now = timer.now();
            float delta = now - lastTick;
            lastTick = now;

            gLevel.beginTick();

            if (pEnemyObject != nullptr) {
                updateEnemy(delta);
            }

            if (pPlayerObject != nullptr) {
                updatePlayer(delta);
            }

            gLevel.useEachObject([&](IGameObject *pObject) {
                if (!isObjectInBounds(pObject))
                    gLevel.deleteObject(pObject);
                else
                    pObject->tick(delta);
            });

            gLevel.endTick();
        }
    });
}

static void createLevel() {
    createAlien("alien");
    pEnemyObject->position = float3::from(2.f, gWorld.getAlienSpawn());
    pEnemyObject->rotation = { -90 * kDegToRad<float>, 0.f, 0.f };
    pEnemyObject->scale = gWorld.getWorldScale();

    createPlayer("player");
    pPlayerObject->position = float3::from(1.f, gWorld.getPlayerSpawn());
    pPlayerObject->rotation = { -90 * kDegToRad<float>, 0.f, 0.f };
    pPlayerObject->scale = gWorld.getWorldScale();

    gLevel.cameraPosition = { 10.f, float(gWorld.width) / 2, float(gWorld.height) / 2 };
    gLevel.cameraRotation = { -1, 0.f, 0.f };

    CrossObject *pCross = addCross("cross");
    pCross->position = float3::from(0.f, 0.f, 0.f);
    pCross->rotation = float3::from(-90 * kDegToRad<float>, 0.f, 0.f);
    pCross->scale = float3::from(0.5f);

    for (size_t i = 0; i < gWorld.maxLives; i++) {
        auto *pLife = gLevel.addObject<PlayerObject>(std::format("life-{}", i));
        pLife->position = gWorld.getWorldPos(float(gWorld.width - i), -1.f);
        pLife->scale = gWorld.getWorldScale();
        pLife->rotation = { -90 * kDegToRad<float>, 0.f, 0.f };
        gWorld.pLifeObjects.push_back(pLife);
    }

    pMainQueue->add("start-game", createGameThread);
}

///
/// entry point
///

static void commonMain(const std::filesystem::path& path) {
    GdkInit gdkInit;

    pWorkThread = new tasks::WorkThread(64, "work");

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

    gLevel.pProjection = gProjections[gCurrentProjection];

    // move the render context into the render thread to prevent hangs on shutdown
    render::Context *pContext = render::Context::create(renderCreateInfo);
    pRenderThread = newTask("render", [pContext](tasks::WorkQueue *pSelf, std::stop_token token) {
        size_t faultCount = 0;
        size_t faultLimit = 3;

        simcoe::logInfo("render fault limit: {} faults", faultLimit);

        // TODO: this is a little stupid
        try {
            pGraph = new Graph(pContext);
            auto *pBackBuffers = pGraph->addResource<graph::SwapChainHandle>();
            auto *pSceneTarget = pGraph->addResource<graph::SceneTargetHandle>();
            auto *pDepthTarget = pGraph->addResource<graph::DepthTargetHandle>();

            auto *pPlayerTexture = pGraph->addResource<graph::TextureHandle>("player.png");
            auto *pCrossTexture = pGraph->addResource<graph::TextureHandle>("cross.png");
            auto *pAlienTexture = pGraph->addResource<graph::TextureHandle>("alien.png");

            pPlayerMesh = pGraph->addObject<ObjMesh>("ship.model");
            pGridMesh = pGraph->addObject<ObjMesh>("grid.model");
            pAlienMesh = pGraph->addObject<ObjMesh>("alien.model");

            pBulletMesh = pGraph->addObject<ObjMesh>("bullet.model");

            pEggSmallMesh = pGraph->addObject<ObjMesh>("egg-small.model");
            pEggMediumMesh = pGraph->addObject<ObjMesh>("egg-medium.model");
            pEggLargeMesh = pGraph->addObject<ObjMesh>("egg-large.model");

            const graph::GameRenderInfo gameRenderConfig = {
                .pCameraUniform = pGraph->addResource<graph::CameraUniformHandle>()
            };

            pGraph->addPass<graph::ScenePass>(pSceneTarget->as<IRTVHandle>());

            auto *pGamePass = pGraph->addPass<graph::GameLevelPass>(&gLevel, pSceneTarget->as<IRTVHandle>(), pDepthTarget->as<IDSVHandle>(), gameRenderConfig);

            //pGraph->addPass<graph::PostPass>(pBackBuffers->as<IRTVHandle>(), pSceneTarget->as<ISRVHandle>());
            pGraph->addPass<GameGui>(pBackBuffers->as<IRTVHandle>(), pSceneTarget->as<ISRVHandle>());
            pGraph->addPass<graph::PresentPass>(pBackBuffers);

            gPlayerTextureId = pGamePass->addTexture(pPlayerTexture);
            gCrossTextureId = pGamePass->addTexture(pCrossTexture);
            gAlienTextureId = pGamePass->addTexture(pAlienTexture);

            gBulletTextureId = gPlayerTextureId;
            gEggSmallTextureId = gAlienTextureId;
            gEggMediumTextureId = gAlienTextureId;
            gEggLargeTextureId = gAlienTextureId;

            pMainQueue->add("create-level", createLevel);

            // TODO: if the render loop throws an exception, the program will std::terminate
            // we should handle this case and restart the render loop
            while (!token.stop_requested()) {
                if (pSelf->process()) {
                    continue; // TODO: this is also a little stupid
                }

                try {
                    pGraph->execute();
                } catch (std::runtime_error& err) {
                    simcoe::logError("render exception: {}", err.what());

                    faultCount += 1;
                    simcoe::logError("render fault. {} total fault{}", faultCount, faultCount > 1 ? "s" : "");
                    if (faultCount > faultLimit) {
                        simcoe::logError("render thread fault limit reached. exiting");
                        break;
                    }

                    pGraph->resumeFromFault();
                } catch (...) {
                    simcoe::logError("unknown thread exception. exiting");
                    break;
                }
            }
        } catch (std::runtime_error& err) {
            simcoe::logError("render thread exception during startup: {}", err.what());
        }

        pMainQueue->add("render-thread-stopped", [] {
            pGraph->setFullscreen(false);
            delete pGraph;

            gRunning = false;
        });
    });

    std::jthread inputThread([](auto token) {
        setThreadName("input");

        while (!token.stop_requested()) {
            pInput->poll();
        }
    });

    while (pSystem->getEvent()) {
        pSystem->dispatchEvent();
        pMainQueue->process();

        if (!gRunning) {
            break;
        }
    }

    delete pGameThread;
    delete pWorkThread;
    delete pRenderThread;
    delete pMainQueue;
}

static fs::path getGameDir() {
    char gamePath[1024];
    GetModuleFileNameA(nullptr, gamePath, sizeof(gamePath));
    return fs::path(gamePath).parent_path();
}

static int innerMain() try {
    setThreadName("main");
    simcoe::addSink(&gFileLogger);
    simcoe::addSink(&gGuiLogger);

    auto mainWorkQueue = std::make_unique<tasks::WorkQueue>(64);
    pMainQueue = mainWorkQueue.get();

    // dont use a Region here because we dont want to print `shutdown` if an exception is thrown
    simcoe::logInfo("startup");
    commonMain(getGameDir());
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
