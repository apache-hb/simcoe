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

    float getMoveStrafe() const {
        return getButtonAxis(Button::eKeyA, Button::eKeyD);
    }

    float getMoveForward() const {
        return getButtonAxis(Button::eKeyS, Button::eKeyW);
    }

    float getMoveVertical() const {
        return getButtonAxis(Button::eKeyQ, Button::eKeyE);
    }

    float getLookHorizontal() const {
        return getButtonAxis(Button::eKeyLeft, Button::eKeyRight);
    }

    float getLookVertical() const {
        return getButtonAxis(Button::eKeyDown, Button::eKeyUp);
    }
};


static GameInputClient gInputClient;
static GameWindow gWindowCallbacks;

struct PlayerEntity : public IEntity { using IEntity::IEntity; };
struct AlienEntity : public IEntity { using IEntity::IEntity; };
struct CameraEntity : public IEntity { using IEntity::IEntity; };

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
        float3 tr = rotation;
        float3 ts = scale;

        auto& queue = GameService::getWorkQueue();

        if (ImGui::DragFloat3("position", tp.data(), 0.1f)) {
            queue.add("update transform", [this, tp] {
                mt::WriteLock lock(GameService::getWorldMutex());
                position = tp;
            });
        }

        if (ImGui::DragFloat3("rotation", tr.data(), 0.1f)) {
            queue.add("update transform", [this, tr] {
                mt::WriteLock lock(GameService::getWorldMutex());
                rotation = tr;
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
        float3 tr = direction;

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
                direction = tr;
            });
        }
    }

    float3 position;
    float3 direction;

    float yaw = -90.f;
    float pitch = 0.f;

    float sensitivity = 1.f;
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
static AlienEntity *gAlien = nullptr;
static CameraEntity *gCamera = nullptr;

static void initEntities(game::World& world) {
    world.onCreate<TransformComp>([](TransformComp *pTransform) {
        IEntity *pEntity = pTransform->getEntity();
        World *pWorld = pEntity->getWorld();

        pWorld->component<GpuTransformComp>(pEntity, pTransform);
    });

    world.onCreate<OrthoCameraComp>([](OrthoCameraComp *pCamera) {
        IEntity *pEntity = pCamera->getEntity();
        World *pWorld = pEntity->getWorld();

        pWorld->component<GpuOrthoCameraComp>(pEntity, pCamera);
    });

    gPlayer = world.entity<PlayerEntity>("player")
        .add<MeshComp>("ship.model")
        .add<TextureComp>("player.png")
        .add<TransformComp>(0.f, 0.f, 1.f);

    gAlien = world.entity<AlienEntity>("alien")
        .add<MeshComp>("alien.model")
        .add<TextureComp>("alien.png")
        .add<TransformComp>(0.f, 0.f, 1.f);

    gCamera = world.entity<CameraEntity>("camera")
        .add<OrthoCameraComp>(float3(0.f, 1.f, 0.f));
}

static void runSystems(game::World& world, SM_UNUSED float delta) {
    auto& workQueue = GameService::getWorkQueue();
    for (size_t i = 0; i < 16 && workQueue.tryGetMessage(); i++) { }

    mt::ReadLock lock(GameService::getWorldMutex());
    // LOG_INFO("=== update ===");

    // if (PlayerEntity *pPlayer = world.get<PlayerEntity>(gPlayer->getInstanceId())) {
    //     LOG_INFO("player: {} (delta {})", pPlayer->getName(), delta);
    // }

    // if (AlienEntity *pAlien = world.get<AlienEntity>(gAlien->getInstanceId())) {
    //     LOG_INFO("alien: {} (delta {})", pAlien->getName(), delta);
    // }

    // LOG_INFO("=== render ===");
    
    float strafe = gInputClient.getMoveStrafe();
    float forward = gInputClient.getMoveForward();
    float moveUD = gInputClient.getMoveVertical();

    float lookLR = gInputClient.getLookHorizontal();
    float lookUD = gInputClient.getLookVertical();

    float cameraSpeed = 10.f;
    float deltaCamera = delta * cameraSpeed;

    game_render::CommandBatch batch;

    if (CameraEntity *pCamera = world.get<CameraEntity>(gCamera->getInstanceId())) {
        OrthoCameraComp *pCameraComp = pCamera->get<OrthoCameraComp>();
        GpuOrthoCameraComp *pGpuCameraComp = pCamera->get<GpuOrthoCameraComp>();

        pCameraComp->position += pCameraComp->direction * deltaCamera * strafe;
        pCameraComp->position += float3::cross(pCameraComp->direction, kWorldUp).normal() * deltaCamera * forward;
        pCameraComp->position += kWorldUp * deltaCamera * moveUD;

        pCameraComp->yaw += lookLR * pCameraComp->sensitivity;
        pCameraComp->pitch += lookUD * pCameraComp->sensitivity;

        if (pCameraComp->pitch > 89.f) {
            pCameraComp->pitch = 89.f;
        } else if (pCameraComp->pitch < -89.f) {
            pCameraComp->pitch = -89.f;
        }

        float3 direction;
        direction.x = cosf(pCameraComp->yaw * math::kDegToRad<float>) * cosf(pCameraComp->pitch * math::kDegToRad<float>);
        direction.y = sinf(pCameraComp->yaw * math::kDegToRad<float>) * cosf(pCameraComp->pitch * math::kDegToRad<float>);
        direction.z = sinf(pCameraComp->pitch * math::kDegToRad<float>);
        pCameraComp->direction = direction.normal();

        batch.add([pGpuCameraComp, pCameraComp](game_render::ScenePass *pScene, Context *pContext) {
            auto *pCommands = pContext->getDirectCommands();

            auto display = pContext->getCreateInfo();
            auto width = display.renderWidth;
            auto height = display.renderHeight;

            float aspect = float(width) / float(height);

            float4x4 view = float4x4::lookAtRH(pCameraComp->position, 0.f, kWorldUp);
            float4x4 proj = float4x4::ortho(0.f, 20.f * aspect, 0.f, 20.f, 0.1f, 100.f);

            auto *pBuffer = pGpuCameraComp->pCameraUniform->getInner();
            auto *pHeap = pContext->getSrvHeap();

            game_render::Camera camera = {
                .view = view,
                .proj = proj
            };
            pBuffer->update(&camera);

            pCommands->setGraphicsShaderInput(pScene->cameraReg(), pHeap->deviceOffset(pBuffer->getSrvIndex()));
        });
    }

    for (IEntity *pEntity : world.allWith<GpuTransformComp, MeshComp>()) {
        GpuTransformComp *pTransformComp = pEntity->get<GpuTransformComp>();
        MeshComp *pMeshComp = pEntity->get<MeshComp>();
        TextureComp *pTextureComp = pEntity->get<TextureComp>();

        batch.add([pMeshComp, pTransformComp, pTextureComp](game_render::ScenePass *pScene, Context *pContext) {
            TransformComp *pTransform = pTransformComp->pTransform;
            auto *pCommands = pContext->getDirectCommands();
            auto *pMesh = pMeshComp->pMesh;
            pCommands->setVertexBuffer(pMesh->getVertexBuffer());
            pCommands->setIndexBuffer(pMesh->getIndexBuffer());

            auto *pBuffer = pTransformComp->pModel->getInner();
            auto *pTexture = pTextureComp->pTexture->getInner();
            auto *pHeap = pContext->getSrvHeap();

            game_render::Model model = {
                .model = float4x4::transform(pTransform->position, pTransform->rotation, pTransform->scale)
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
