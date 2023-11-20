// core
#include "editor/graph/mesh.h"
#include "engine/core/mt.h"

// math
#include "engine/math/format.h"

// logging
#include "engine/log/sinks.h"

// services
#include "engine/service/service.h"
#include "engine/debug/service.h"
#include "engine/service/platform.h"
#include "engine/service/freetype.h"
#include "engine/log/service.h"
#include "engine/threads/service.h"
#include "engine/depot/service.h"
#include "engine/input/service.h"
#include "engine/config/service.h"
#include "engine/audio/service.h"
#include "engine/rhi/service.h"
#include "engine/render/service.h"
#include "game/service.h"

// threads
#include "engine/threads/schedule.h"
#include "engine/threads/queue.h"

// util
#include "engine/util/time.h"

// input
#include "engine/input/win32-device.h"
#include "engine/input/xinput-device.h"

// render passes
#include "editor/graph/assets.h"
#include "editor/graph/post.h"
#include "editor/graph/gui.h"

// imgui
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include "imfiles/imfilebrowser.h"
#include "implot/implot.h"

// editor ui
#include "editor/service.h"
#include "editor/ui/ui.h"
#include "editor/ui/panels/threads.h"
#include "editor/ui/panels/depot.h"
#include "editor/ui/panels/logging.h"

// vendor
#include "vendor/gameruntime/service.h"
#include "vendor/ryzenmonitor/service.h"

// game
#include "game/ecs/world.h"

using namespace simcoe;
using namespace simcoe::math;

using namespace game;

using namespace editor;

namespace game_render = game::render;

using microsoft::GdkService;
using amd::RyzenMonitorSerivce;

// we use a z up right handed coordinate system

constexpr float3 kWorldUp = float3(0.f, 0.f, 1.f); // z up
constexpr float3 kWorldForward = float3(0.f, 1.f, 0.f); // y forward
constexpr float3 kWorldRight = float3(1.f, 0.f, 0.f); // x right

static std::atomic_bool bRunning = true;

struct GameWindow final : IWindowCallbacks {
    void onClose() override {
        bRunning = false;

        RenderService::shutdown();
        PlatformService::quit();
    }

    void onResize(const WindowSize& event) override {
        static bool bFirstEvent = true;
        if (bFirstEvent) {
            bFirstEvent = false;
            return;
        }

        EditorService::resizeDisplay(event);
    }

    bool onEvent(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override {
        InputService::handleMsg(uMsg, wParam, lParam);
        return graph::IGuiPass::handleMsg(hWnd, uMsg, wParam, lParam);
    }
};

using namespace input;

struct GameInputClient final : public input::IClient {
    using IClient::IClient;

    void onInput(const State& event) override {
        state = event;    

        quitEventKey.update(state.buttons[Button::eKeyEscape]);
        quitEventGamepad.update(state.buttons[Button::ePadBack]);

        shootKeyboardEvent.update(state.buttons[Button::eKeySpace]);
        shootGamepadEvent.update(state.buttons[Button::ePadButtonDown]);

        moveUpEventKey.update(state.buttons[Button::eKeyW]);
        moveDownEventKey.update(state.buttons[Button::eKeyS]);
        moveLeftEventKey.update(state.buttons[Button::eKeyA]);
        moveRightEventKey.update(state.buttons[Button::eKeyD]);

        moveUpEventArrow.update(state.buttons[Button::eKeyUp]);
        moveDownEventArrow.update(state.buttons[Button::eKeyDown]);
        moveLeftEventArrow.update(state.buttons[Button::eKeyLeft]);
        moveRightEventArrow.update(state.buttons[Button::eKeyRight]);

        moveUpEventPad.update(state.buttons[Button::ePadDirectionUp]);
        moveDownEventPad.update(state.buttons[Button::ePadDirectionDown]);
        moveLeftEventPad.update(state.buttons[Button::ePadDirectionLeft]);
        moveRightEventPad.update(state.buttons[Button::ePadDirectionRight]);
    }

    State state;

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

    bool isShootPressed() const {
        return shootKeyboardEvent.isPressed()
            || shootGamepadEvent.isPressed();
    }

    bool isQuitPressed() const {
        return quitEventKey.isPressed()
            || quitEventGamepad.isPressed();
    }

    bool consumeMoveUp() {
        return moveUpEventKey.beginPress()
            || moveUpEventArrow.beginPress()
            || moveUpEventPad.beginPress();
    }

    bool consumeMoveDown() {
        return moveDownEventKey.beginPress()
            || moveDownEventArrow.beginPress()
            || moveDownEventPad.beginPress();
    }

    bool consumeMoveLeft() {
        return moveLeftEventKey.beginPress()
            || moveLeftEventArrow.beginPress()
            || moveLeftEventPad.beginPress();
    }

    bool consumeMoveRight() {
        return moveRightEventKey.beginPress()
            || moveRightEventArrow.beginPress()
            || moveRightEventPad.beginPress();
    }

    float getMoveHorizontal() const {
        return getButtonAxis(Button::eKeyA, Button::eKeyD);
    }

    float getMoveVertical() const {
        return getButtonAxis(Button::eKeyS, Button::eKeyW);
    }

private:
    Event shootKeyboardEvent;
    Event shootGamepadEvent;

    Event quitEventKey, quitEventGamepad;

    Event moveUpEventKey,    moveUpEventArrow,    moveUpEventPad;
    Event moveDownEventKey,  moveDownEventArrow,  moveDownEventPad;
    Event moveLeftEventKey,  moveLeftEventArrow,  moveLeftEventPad;
    Event moveRightEventKey, moveRightEventArrow, moveRightEventPad;
};


static GameInputClient gInputClient;
static GameWindow gWindowCallbacks;

struct PlayerEntity : public IEntity { using IEntity::IEntity; };
struct AlienShipEntity : public IEntity { using IEntity::IEntity; };
struct CameraEntity : public IEntity { using IEntity::IEntity; };
struct BulletEntity : public IEntity { using IEntity::IEntity; };

constexpr float2 kTileSize = { 1.4f, 1.2f };

struct AlienShipBehaviour : public IComponent {
    using IComponent::IComponent;
    static constexpr const char *kTypeName = "mothership_behaviour";

    /**
     * @param shipSpeed speed of the alien ship in tiles per second
     * @param spawnDelay delay between alien spawns in seconds
     * @param spawnGracePeriod grace period before the first alien spawn in seconds
     */
    AlienShipBehaviour(ComponentData data, float shipSpeed = 0.7f, float spawnDelay = 1.f, float spawnGracePeriod = 1.5f)
        : IComponent(data)
        , moveDelay(shipSpeed)
        , spawnDelay(spawnDelay)
        , lastSpawn(spawnGracePeriod)
    { }

    void onDebugDraw() override {
        ImGui::Text("move delay: %f", moveDelay);
        ImGui::Text("spawn delay: %f", spawnDelay);

        ImGui::ProgressBar(lastMove / moveDelay, ImVec2(0.f, 0.f), "Until next move");
        ImGui::ProgressBar(lastSpawn / spawnDelay, ImVec2(0.f, 0.f), "Until next spawn");
    }

    float moveDelay;
    float lastMove = 0.f;

    float spawnDelay;
    float lastSpawn;
};

struct PlayerInputComp : public IComponent {
    using IComponent::IComponent;
    static constexpr const char *kTypeName = "player_input";

    PlayerInputComp(ComponentData data)
        : IComponent(data)
    { }

    bool isShootPressed() const { return gInputClient.isShootPressed(); }
    bool isQuitPressed() const { return gInputClient.isQuitPressed(); }

    bool consumeMoveUp() { return gInputClient.consumeMoveUp(); }
    bool consumeMoveDown() { return gInputClient.consumeMoveDown(); }
    bool consumeMoveLeft() { return gInputClient.consumeMoveLeft(); }
    bool consumeMoveRight() { return gInputClient.consumeMoveRight(); }
};

struct ShootComp : public IComponent {
    using IComponent::IComponent;
    static constexpr const char *kTypeName = "shooting";

    ShootComp(ComponentData data, float delay, float bulletSpeed = 5.f)
        : IComponent(data)
        , shootDelay(delay)
        , bulletSpeed(bulletSpeed)
    { }

    void onDebugDraw() override {
        ImGui::SliderFloat("shoot delay", &shootDelay, 0.f, 1.f, "%.2f");
        ImGui::SliderFloat("bullet speed", &bulletSpeed, 0.f, 10.f, "%.2f");
        ImGui::ProgressBar(lastShot / shootDelay, ImVec2(0.f, 0.f), "Until next shot");
    }

    float shootDelay = 0.3f;
    float lastShot = 0.f;

    float bulletSpeed = 5.f;
};

struct HealthComp : public IComponent {
    using IComponent::IComponent;
    static constexpr const char *kTypeName = "health";

    HealthComp(ComponentData data, int current, int total)
        : IComponent(data)
        , currentHealth(current)
        , maxHealth(total)
    { }

    int currentHealth;
    int maxHealth;
};

struct ProjectileComp : public IComponent {
    using IComponent::IComponent;
    static constexpr const char *kTypeName = "projectile";

    ProjectileComp(ComponentData data, float2 speed, TypeInfo parent)
        : IComponent(data)
        , speed(speed)
        , parent(parent)
    { }

    float2 speed;
    TypeInfo parent;
};

struct IAssetComp : public IComponent {
    IAssetComp(ComponentData data, fs::path path)
        : IComponent(data)
        , path(path)
    { }

    fs::path path;
};

struct MeshComp : public IAssetComp { 
    using IAssetComp::IAssetComp;
    static constexpr const char *kTypeName = "obj_mesh";

    void onCreate() override {
        auto *pGraph = RenderService::getGraph();
        pMesh = pGraph->newGraphObject<graph::ObjMesh>(path);

        LOG_INFO("loaded mesh {}", path.string());
    }

    void onDebugDraw() override {
        ImGui::Text("mesh: %s", path.string().c_str());
        ImGui::Text("index count: %zu", pMesh->getIndexCount());
    }

    graph::ObjMesh *pMesh = nullptr;
};

struct TextureComp : public IAssetComp {
    using IAssetComp::IAssetComp;
    static constexpr const char *kTypeName = "texture";

    void onCreate() override {
        auto *pGraph = RenderService::getGraph();
        pTexture = pGraph->addResource<graph::TextureHandle>(path.string());
    }

    void onDebugDraw() override {
        auto *pData = this->pTexture->getInner();
        auto size = pData->getSize();
        ImGui::Text("texture: %s", path.string().c_str());
        ImGui::Text("size: %ux%u", size.x, size.y);
    }

    ResourceWrapper<graph::TextureHandle> *pTexture = nullptr;
};

// model transform

struct TransformComp : public IComponent { 
    using IComponent::IComponent;
    static constexpr const char *kTypeName = "transform";

    TransformComp(ComponentData data, float3 position = 0.f, float3 rotation = 0.f, float3 scale = 1.f)
        : IComponent(data)
        , position(position)
        , rotation(rotation)
        , scale(scale)
    { }

    void onDebugDraw() override {
        float3 tp = position;
        float3 tr = rotation.degrees();
        float3 ts = scale;

        auto& queue = GameService::getWorkQueue();

        if (ImGui::DragFloat3("position", tp.data(), 0.1f)) {
            queue.add("update transform", [this, tp] {
                mt::WriteLock lock(GameService::getWorldMutex());
                position = tp;
            });
        }

        if (ImGui::DragFloat3("rotation", tr.data(), 5.f)) {
            queue.add("update transform", [this, tr] {
                mt::WriteLock lock(GameService::getWorldMutex());
                rotation = tr.radians();
            });
        }

        if (ImGui::DragFloat3("scale", ts.data(), 0.1f)) {
            queue.add("update transform", [this, ts] {
                mt::WriteLock lock(GameService::getWorldMutex());
                scale = ts;
            });
        }
    }

    float3 position;
    float3 rotation;
    float3 scale;
};

struct GpuTransformComp : public IComponent {
    using IComponent::IComponent;
    static constexpr const char *kTypeName = "gpu_transform";

    GpuTransformComp(ComponentData data, TransformComp *pTransform)
        : IComponent(data)
        , pTransform(pTransform)
    { }

    void onCreate() override {
        auto *pGraph = RenderService::getGraph();
        pModel = pGraph->addResource<game_render::ModelUniform>("uniform.model");
    }

    TransformComp *pTransform = nullptr;
    ResourceWrapper<game_render::ModelUniform> *pModel = nullptr;
};

// camera transform

struct OrthoCameraComp : public IComponent {
    using IComponent::IComponent;
    static constexpr const char *kTypeName = "ortho_camera";

    OrthoCameraComp(ComponentData data, float3 position = 0.f, float3 direction = kWorldForward)
        : IComponent(data)
        , position(position)
        , direction(direction)
    { }

    void onDebugDraw() override {
        float3 tp = position;
        float3 tr = direction.degrees();

        auto& queue = GameService::getWorkQueue();

        ImGui::Text("near: %f", 0.1f);
        ImGui::Text("far: %f", 100.f);

        if (ImGui::DragFloat3("position", tp.data(), 0.1f)) {
            queue.add("update camera", [this, tp] {
                mt::WriteLock lock(GameService::getWorldMutex());
                position = tp;
            });
        }

        if (ImGui::DragFloat3("direction", tr.data(), 0.1f)) {
            queue.add("update camera", [this, tr] {
                mt::WriteLock lock(GameService::getWorldMutex());
                direction = tr.radians();
            });
        }
    }

    float3 position;
    float3 direction;
};

struct GpuOrthoCameraComp : public IComponent {
    using IComponent::IComponent;
    static constexpr const char *kTypeName = "gpu_ortho_camera";

    GpuOrthoCameraComp(ComponentData data, OrthoCameraComp *pCamera)
        : IComponent(data)
        , pCamera(pCamera)
    { }

    void onCreate() override {
        auto *pGraph = RenderService::getGraph();
        pCameraUniform = pGraph->addResource<game_render::CameraUniform>("uniform.camera");
    }

    OrthoCameraComp *pCamera = nullptr;
    ResourceWrapper<game_render::CameraUniform> *pCameraUniform = nullptr;
};

static PlayerEntity *gPlayer = nullptr;
static AlienShipEntity *gAlien = nullptr;
static CameraEntity *gCamera = nullptr;

static MeshComp *gGridMesh = nullptr;
static MeshComp *gAlienMesh = nullptr;
static MeshComp *gBulletMesh = nullptr;
static MeshComp *gPlayerMesh = nullptr;

static TextureComp *gGridTexture = nullptr;
static TextureComp *gAlienTexture = nullptr;
static TextureComp *gBulletTexture = nullptr;
static TextureComp *gPlayerTexture = nullptr;

static void initEntities(game::World& world) {
    world.onCreate<TransformComp>([](TransformComp *pTransform) {
        World *pWorld = pTransform->getWorld();

        auto *pGpu = pWorld->component<GpuTransformComp>(pTransform);

        pTransform->associate(pGpu);
    });

    world.onCreate<OrthoCameraComp>([](OrthoCameraComp *pCamera) {
        World *pWorld = pCamera->getWorld();

        auto *pGpu = pWorld->component<GpuOrthoCameraComp>(pCamera);
        pCamera->associate(pGpu);
    });

    gGridMesh = world.component<MeshComp>("grid.model");
    gAlienMesh = world.component<MeshComp>("alien.model");
    gBulletMesh = world.component<MeshComp>("bullet.model");
    gPlayerMesh = world.component<MeshComp>("ship.model");

    gGridTexture = world.component<TextureComp>("cross.png");
    gAlienTexture = world.component<TextureComp>("alien.png");
    gBulletTexture = world.component<TextureComp>("player.png");
    gPlayerTexture = world.component<TextureComp>("player.png");

    gPlayer = world.entity<PlayerEntity>("player")
        .add<PlayerInputComp>()
        .add<ShootComp>(0.3f)
        .add<HealthComp>(3, 5)
        .add(gPlayerMesh).add(gPlayerTexture)
        .add<TransformComp>(float3(0.f, 0.f, 20.4f), float3(-90.f, 0.f, 90.f).radians(), 0.5f);

    gAlien = world.entity<AlienShipEntity>("alien")
        .add<AlienShipBehaviour>(0.7f, 1.f, 1.5f)
        .add(gAlienMesh).add(gAlienTexture)
        .add<TransformComp>(float3(0.f, 0.f, 21.6f), float3(-90.f, 90.f, 0.f).radians(), 0.6f);

    gCamera = world.entity<CameraEntity>("camera")
        .add<OrthoCameraComp>(float3(14.f, -10.f, 10.6f), (kWorldForward * 90.f).radians());

    world.entity<IEntity>("grid")
        .add(gGridMesh).add(gGridTexture)
        // scale is non-uniform to emulate original the vic20 display being non-square
        .add<TransformComp>(float3(0.f, 1.f, 0.f), float3(-90.f, 90.f, 0.f).radians(), float3(0.7f, 0.6f, 0.7f)); 
}

constexpr float2 kWorldBounds = float2(30.f, 21.f);

// use special bounds for the bullet to account for the edges
static bool isBulletInBounds(float2 pos) {
    return pos.x >= -0.5f && pos.x <= kWorldBounds.x
        && pos.y >= -0.5f && pos.y <= kWorldBounds.y + 1.f;
}

static void runSystems(game::World& world, float delta) {
    auto& workQueue = GameService::getWorkQueue();
    for (size_t i = 0; i < 16 && workQueue.tryGetMessage(); i++) { }

    mt::WriteLock lock(GameService::getWorldMutex());
    // LOG_INFO("=== update ===");

    // do movement and shooting input
    for (IEntity *pEntity : world.allWith<PlayerInputComp, ShootComp, TransformComp>()) {
        PlayerInputComp *pInput = pEntity->get<PlayerInputComp>();
        TransformComp *pTransform = pEntity->get<TransformComp>();
        ShootComp *pShoot = pEntity->get<ShootComp>();

        float3& pos = pTransform->position;

        float mv = 0.f, mh = 0.f;

        if (pInput->consumeMoveDown()) {
            mv = -kTileSize.y;
        } else if (pInput->consumeMoveUp()) {
            mv = kTileSize.y;
        }

        if (pInput->consumeMoveLeft()) {
            mh = -kTileSize.x;
        } else if (pInput->consumeMoveRight()) {
            mh = kTileSize.x;
        }

        // we clamp differently here to maintain the player origin offset
        // so we line up with the grid
        float3 np = pos + float3(mh, 0.f, mv);
        if (-0.3f > np.x || kWorldBounds.x < np.x) {
            np.x = pos.x;
        }

        if (-0.3f > np.z || kWorldBounds.y < np.z) {
            np.z = pos.z;
        }

        pos = np;

        float angle = std::atan2(mv, mh);

        if (mv != 0.f || mh != 0.f) {
            pTransform->rotation.x = -angle; // = float3(0.f, -angle, 0.f);
        }

        pShoot->lastShot += delta;

        if (pInput->isShootPressed()) {
            if (pShoot->lastShot > pShoot->shootDelay) {
                pShoot->lastShot = 0.f;

                float playerAngle = -pTransform->rotation.x;

                workQueue.add("bullet", [playerAngle, pTransform, pEntity, speed = pShoot->bulletSpeed] {
                    float2 direction = float2(std::cos(playerAngle), std::sin(playerAngle));

                    World *pWorld = pEntity->getWorld();

                    pWorld->entity<BulletEntity>("bullet")
                        .add(gBulletMesh).add(gBulletTexture)
                        .add<TransformComp>(pTransform->position, pTransform->rotation, 0.2f)
                        .add<ProjectileComp>(direction * speed, pEntity->getTypeInfo());
                });
            }
        }
    }

    // do bullet movement
    for (IEntity *pEntity : world.allWith<ProjectileComp, TransformComp>()) {
        ProjectileComp *pProjectile = pEntity->get<ProjectileComp>();
        TransformComp *pTransform = pEntity->get<TransformComp>();

        pTransform->position.x += pProjectile->speed.x * delta;
        pTransform->position.z += pProjectile->speed.y * delta;

        if (!isBulletInBounds(pTransform->position.xz())) {
            workQueue.add("delete", [pEntity] { 
                World *pWorld = pEntity->getWorld();
                pWorld->destroy(pEntity);
            });
        }
    }


    // move the mothership
    for (IEntity *pEntity : world.allWith<AlienShipBehaviour, TransformComp>()) {
        AlienShipBehaviour *pBehaviour = pEntity->get<AlienShipBehaviour>();
        TransformComp *pTransform = pEntity->get<TransformComp>();

        pBehaviour->lastMove += delta;
        pBehaviour->lastSpawn += delta;

        if (pBehaviour->lastMove >= pBehaviour->moveDelay) {
            pBehaviour->lastMove = 0.f;
            pTransform->position.x += kTileSize.x;
        }

        if (pTransform->position.x > kWorldBounds.x) {
            pTransform->position.x = 0.f;
        }
    }

    // LOG_INFO("=== render ===");
    
    game_render::CommandBatch batch;

    if (CameraEntity *pCamera = world.get<CameraEntity>(gCamera->getInstanceId())) {
        OrthoCameraComp *pCameraComp = pCamera->get<OrthoCameraComp>();
        GpuOrthoCameraComp *pGpuCameraComp = pCameraComp->associated<GpuOrthoCameraComp>();

        batch.add([pGpuCameraComp, pCameraComp](game_render::ScenePass *pScene, Context *pContext) {
            auto *pCommands = pContext->getDirectCommands();

            auto display = pContext->getCreateInfo();
            auto width = display.renderWidth;
            auto height = display.renderHeight;

            float aspect = float(width) / float(height);

            float4x4 view = float4x4::lookToRH(pCameraComp->position, pCameraComp->direction, kWorldUp);
            float4x4 proj = float4x4::orthographicRH(24.f * aspect, 24.f, 0.1f, 100.f);

            auto *pBuffer = pGpuCameraComp->pCameraUniform->getInner();
            auto *pHeap = pContext->getSrvHeap();

            game_render::Camera camera = {
                .view = view.transpose(),
                .proj = proj.transpose()
            };
            pBuffer->update(&camera);

            pCommands->setGraphicsShaderInput(pScene->cameraReg(), pHeap->deviceOffset(pBuffer->getSrvIndex()));
        });
    }

    for (IEntity *pEntity : world.allWith<TransformComp, MeshComp>()) {
        TransformComp *pTransformComp = pEntity->get<TransformComp>();
        MeshComp *pMeshComp = pEntity->get<MeshComp>();
        TextureComp *pTextureComp = pEntity->get<TextureComp>();

        batch.add([pMeshComp, pTransformComp, pTextureComp](game_render::ScenePass *pScene, Context *pContext) {
            GpuTransformComp *pGpuTransformComp = pTransformComp->associated<GpuTransformComp>();
            auto *pCommands = pContext->getDirectCommands();
            auto *pMesh = pMeshComp->pMesh;
            pCommands->setVertexBuffer(pMesh->getVertexBuffer());
            pCommands->setIndexBuffer(pMesh->getIndexBuffer());

            auto *pBuffer = pGpuTransformComp->pModel->getInner();
            auto *pTexture = pTextureComp->pTexture->getInner();
            auto *pHeap = pContext->getSrvHeap();

            game_render::Model model = {
                .model = float4x4::transform(pTransformComp->position, pTransformComp->rotation, pTransformComp->scale)
            };
            pBuffer->update(&model);

            pCommands->setGraphicsShaderInput(pScene->textureReg(), pHeap->deviceOffset(pTexture->getSrvIndex()));
            pCommands->setGraphicsShaderInput(pScene->modelReg(), pHeap->deviceOffset(pBuffer->getSrvIndex()));

            pCommands->drawIndexBuffer(pMesh->getIndexCount());
        });
    }

    GameService::getScene()
        ->update(std::move(batch));
}

///
/// entry point
///

using namespace std::chrono_literals;

static void commonMain() {
    debug::setThreadName("main");
    EditorService::start();
    RenderService::start();
    InputService::addClient(&gInputClient);

    game::World& world = GameService::getWorld();

    initEntities(world);

    Clock clock;
    float last = 0.f;

    while (bRunning) {
        ThreadService::pollMain();

        float delta = clock.now() - last;
        last = clock.now();
        runSystems(world, delta);
        std::this_thread::sleep_for(15ms);
    }
}

static int serviceWrapper() try {
    LoggingService::addSink(EditorService::addDebugService<ui::LoggingUi>());

    auto engineServices = std::to_array({
        PlatformService::service(),
        LoggingService::service(),
        InputService::service(),
        DepotService::service(),
        AudioService::service(),
        FreeTypeService::service(),
        GpuService::service(),
        RenderService::service(),
        GameService::service(),
        EditorService::service(),

        GdkService::service(),
        RyzenMonitorSerivce::service()
    });
    ServiceRuntime runtime{engineServices};

    commonMain();
    LOG_INFO("no game exceptions have occured during runtime");

    return 0;
} catch (const core::Error& err) {
    LOG_ERROR("unhandled exception: {}", err.what());
    return 99;
} catch (...) {
    LOG_ERROR("unhandled exception");
    return 99;
}

static int innerMain() {
    threads::setThreadName("main");

    LOG_INFO("bringing up services");
    int result = serviceWrapper();
    LOG_INFO("all services shut down gracefully");
    return result;
}

// gui entry point

int wWinMain(HINSTANCE hInstance, SM_UNUSED HINSTANCE hPrevInstance, SM_UNUSED LPWSTR lpCmdLine, int nCmdShow) {
    PlatformService::setup(hInstance, nCmdShow, &gWindowCallbacks);
    return innerMain();
}

// command line entry point

int main(SM_UNUSED int argc, SM_UNUSED const char **argv) {
    PlatformService::setup(GetModuleHandle(nullptr), SW_SHOWDEFAULT, &gWindowCallbacks);
    return innerMain();
}
