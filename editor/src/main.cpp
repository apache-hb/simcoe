#define _CRT_SECURE_NO_WARNINGS 1

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
#include <random>

using namespace simcoe;
using namespace simcoe::math;

using namespace game;

using namespace editor;

namespace game_render = game::render;
namespace editor_ui = editor::ui;
namespace game_ui = game::ui;

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

std::atomic_int32_t gScore = 0;

static GameInputClient gInputClient;
static GameWindow gWindowCallbacks;

struct CameraEntity : public IEntity { using IEntity::IEntity; };

// asset types

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

static std::unordered_multimap<audio::SoundFormat, audio::VoiceHandlePtr> gVoices = {};

struct AudioComp : public IAssetComp {
    using IAssetComp::IAssetComp;
    static constexpr const char *kTypeName = "audio";

    AudioComp(ComponentData data, fs::path path, float volume = 1.f)
        : IAssetComp(data, path)
        , volume(volume)
    { }

    void onCreate() override {
        auto file = DepotService::openFile(path);
        pSoundBuffer = AudioService::loadVorbisOgg(file);
    }

    void onDebugDraw() override {
        ImGui::Text("audio: %s", path.string().c_str());
        ImGui::SliderFloat("volume", &volume, 0.f, 1.f, "%.2f");
    }

    void playSound() {
        auto [front, back] = gVoices.equal_range(pSoundBuffer->getFormat());
        for (auto it = front; it != back; it++) {
            auto *pVoice = it->second.get();
            if (!pVoice->isPlaying()) {
                pVoice->setVolume(volume);
                pVoice->submit(pSoundBuffer);
                return;
            }
        }

        auto pVoice = AudioService::createVoice("effect", pSoundBuffer->getFormat());
        pVoice->setVolume(volume);
        pVoice->submit(pSoundBuffer);
        gVoices.emplace(pSoundBuffer->getFormat(), pVoice);
    }

    audio::SoundBufferPtr pSoundBuffer = nullptr;
    float volume;
};

// assets

static MeshComp *gGridMesh = nullptr;
static MeshComp *gAlienMesh = nullptr;
static MeshComp *gBulletMesh = nullptr;
static MeshComp *gPlayerMesh = nullptr;

static MeshComp *gEggSmallMesh = nullptr;
static MeshComp *gEggMediumMesh = nullptr;
static MeshComp *gEggLargeMesh = nullptr;

static TextureComp *gGridTexture = nullptr;
static TextureComp *gAlienTexture = nullptr;
static TextureComp *gBulletTexture = nullptr;
static TextureComp *gPlayerTexture = nullptr;

static AudioComp *gShootSound = nullptr;
static AudioComp *gAlienDeathSound = nullptr;
static AudioComp *gPlayerHitSound = nullptr;
static AudioComp *gPlayerDeathSound = nullptr;

static AudioComp *gEggSpawnSound = nullptr;
static AudioComp *gEggGrowMediumSound = nullptr;
static AudioComp *gEggGrowLargeSound = nullptr;
static AudioComp *gEggDeathSound = nullptr;
static AudioComp *gEggHatchSound = nullptr;

// behaviours

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

enum EggState { eEggSmall, eEggMedium, eEggLarge };

struct EggBehaviour : public IComponent {
    EggBehaviour(ComponentData data, float timeToGrowMedium, float timeToGrowLarge, float timeToHatch)
        : IComponent(data)
        , timeToGrowMedium(timeToGrowMedium)
        , timeToGrowLarge(timeToGrowLarge)
        , timeToHatch(timeToHatch)
    { }

    EggState state = eEggSmall;

    float timeToGrowMedium;
    float timeToGrowLarge;
    float timeToHatch;

    float currentTimeAlive = 0.f;
};

struct SwarmBehaviour : public IComponent {
    SwarmBehaviour(ComponentData data, float2 direction, float timeToMove)
        : IComponent(data)
        , direction(direction)
        , timeToMove(timeToMove)
    { }

    float2 direction;

    float timeToMove;
    float lastMove = 0.f;
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

struct HealthComp : public IComponent {
    using IComponent::IComponent;
    static constexpr const char *kTypeName = "health";

    HealthComp(ComponentData data, int current, int total, AudioComp *pHitSound, AudioComp *pDeathSound, bool bIsPlayer = false)
        : IComponent(data)
        , currentHealth(current)
        , maxHealth(total)
        , bIsPlayer(bIsPlayer)
        , pHitSound(pHitSound)
        , pDeathSound(pDeathSound)
    { }

    void takeHit() {
        if (currentHealth > 0) {
            currentHealth--;

            if (pHitSound) pHitSound->playSound();
        }

        if (currentHealth <= 0) {
            if (pDeathSound) pDeathSound->playSound();
        }
    }

    bool isAlive() const { return currentHealth > 0; }

    int currentHealth;
    int maxHealth;

    bool bIsPlayer = false;

    AudioComp *pHitSound = nullptr;
    AudioComp *pDeathSound = nullptr;
};

struct ProjectileComp : public IComponent {
    using IComponent::IComponent;
    static constexpr const char *kTypeName = "projectile";

    ProjectileComp(ComponentData data, float2 speed)
        : IComponent(data)
        , speed(speed)
    { }

    float2 speed;
};

struct ShootComp : public IComponent {
    using IComponent::IComponent;
    static constexpr const char *kTypeName = "shooting";

    ShootComp(ComponentData data, float delay, float bulletSpeed, AudioComp *pSound)
        : IComponent(data)
        , shootDelay(delay)
        , bulletSpeed(bulletSpeed)
        , pSound(pSound)
    { }

    void onDebugDraw() override {
        ImGui::SliderFloat("shoot delay", &shootDelay, 0.f, 1.f, "%.2f");
        ImGui::SliderFloat("bullet speed", &bulletSpeed, 0.f, 10.f, "%.2f");
        ImGui::ProgressBar(lastShot / shootDelay, ImVec2(0.f, 0.f), "Until next shot");
    }

    float shootDelay = 0.3f;
    float lastShot = 0.f;

    float bulletSpeed = 5.f;

    AudioComp *pSound = nullptr;
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

static CameraEntity *gCamera = nullptr;
static IEntity *gPlayerEntity = nullptr;

static AudioComp *gSwarmNoise1 = nullptr;
static AudioComp *gSwarmNoise2 = nullptr;
static AudioComp *gSwarmNoise3 = nullptr;
static AudioComp *gSwarmNoise4 = nullptr;
static AudioComp *gSwarmNoise5 = nullptr;

static std::array<AudioComp*, 5> gSwarmNoiseArr;
int gCurrentNoiseIndex = -1;
int gLowestNoiseIndex = 0;

static void initNoise() {
    gSwarmNoiseArr[0] = gSwarmNoise1;
    gSwarmNoiseArr[1] = gSwarmNoise2;
    gSwarmNoiseArr[2] = gSwarmNoise3;
    gSwarmNoiseArr[3] = gSwarmNoise4;
    gSwarmNoiseArr[4] = gSwarmNoise5;
}

static audio::VoiceHandlePtr gSwarmVoice = nullptr;

static void setNewNoise(int index, bool updateLowest = false) {
    if (gCurrentNoiseIndex == index) return;

    if (updateLowest) gLowestNoiseIndex = index;

    int clamped = std::clamp<int>(index, gLowestNoiseIndex, 4);

    if (gCurrentNoiseIndex == clamped) return;
    gCurrentNoiseIndex = clamped;

    auto *pNoise = gSwarmNoiseArr[gCurrentNoiseIndex];

    gSwarmVoice->reset();
    gSwarmVoice->submit(pNoise->pSoundBuffer);
    gSwarmVoice->resume();
}

float gElapsed = 0.f;
size_t gPlayerHealth = 0;
size_t gCurrentAliveEggs = 0;
size_t gCurrentAliveSwarm = 0;

static void updatePlayingMusic(float delta) {
    if (gPlayerHealth == 0) {
        gSwarmVoice->pause();
        return;
    }

    int chosenNoise = gCurrentNoiseIndex;
    bool bNewLowest = false;

    gElapsed += delta;

    // ramp volume up to .45 over 3 seconds and play the first noise

    // the final 3 tracks are controlled by player health and enemy count
    if (gPlayerHealth == 1) {
        chosenNoise = 4;
        bNewLowest = true;
    } 
    // if theres more than 4 eggs alive move to track 3
    else if (gCurrentAliveEggs > 4 || gCurrentAliveSwarm > 3) {
        chosenNoise = 3;
    } else if (3.f > gElapsed) {
        float volume = std::clamp(gElapsed / 3.f, 0.f, 0.45f);
        gSwarmVoice->setVolume(volume);
        chosenNoise = 0;
        bNewLowest = true;
    } else if (9.f > gElapsed) {
        // after the next 6 seconds move to the new baseline track
        bNewLowest = true;
        chosenNoise = 1;

        // over the next 6 seconds ramp up to full volume
        float volume = std::clamp((gElapsed - 3.f) / 6.f, 0.45f, 1.f);
        gSwarmVoice->setVolume(volume);
    } else if (30.f > gElapsed) {
        bNewLowest = true;
    } 

    setNewNoise(chosenNoise, bNewLowest);
}

enum CurrentScene {
    eGameScene,
    eScoreScene,
    eMenuScene
};

CurrentScene gScene = eGameScene;

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

    gEggSmallMesh = world.component<MeshComp>("egg-small.model");
    gEggMediumMesh = world.component<MeshComp>("egg-medium.model");
    gEggLargeMesh = world.component<MeshComp>("egg-large.model");

    gGridTexture = world.component<TextureComp>("cross.png");
    gAlienTexture = world.component<TextureComp>("alien.png");
    gBulletTexture = world.component<TextureComp>("player.png");
    gPlayerTexture = world.component<TextureComp>("player.png");

    gShootSound = world.component<AudioComp>("pew.ogg", 0.6f);
    gAlienDeathSound = world.component<AudioComp>("alien_kill.ogg", 0.4f);
    gPlayerHitSound = world.component<AudioComp>("damage_hit.ogg");
    gPlayerDeathSound = world.component<AudioComp>("game_over.ogg");

    gEggSpawnSound = world.component<AudioComp>("egg_spawn.ogg", 0.3f);
    gEggGrowMediumSound = world.component<AudioComp>("egg_grow_medium.ogg", 0.6f);
    gEggGrowLargeSound = world.component<AudioComp>("egg_grow_large.ogg");
    gEggDeathSound = world.component<AudioComp>("egg_kill.ogg", 0.7f);
    gEggHatchSound = world.component<AudioComp>("egg_hatch.ogg", 0.7f);

    gSwarmNoise1 = world.component<AudioComp>("swarm1.ogg", 0.3f);
    gSwarmNoise2 = world.component<AudioComp>("swarm2.ogg", 0.4f);
    gSwarmNoise3 = world.component<AudioComp>("swarm3.ogg", 0.6f);
    gSwarmNoise4 = world.component<AudioComp>("swarm4.ogg", 0.7f);
    gSwarmNoise5 = world.component<AudioComp>("swarm5.ogg", 0.9f);

    gSwarmVoice = AudioService::createVoice("swarm", gSwarmNoise5->pSoundBuffer->getFormat());

    initNoise();

    gPlayerHealth = 3;

    gPlayerEntity = world.entity("player")
        .add<PlayerInputComp>()
        .add<ShootComp>(0.3f, 9.f, gShootSound)
        .add<HealthComp>(3, 5, gPlayerHitSound, gPlayerDeathSound, true)
        .add(gPlayerMesh).add(gPlayerTexture)
        .add<TransformComp>(float3(0.f, 0.f, 20.4f), float3(-90.f, 0.f, 90.f).radians(), 0.5f);

    world.entity("alien")
        .add<AlienShipBehaviour>(0.7f, 1.5f, 1.5f)
        .add(gAlienMesh).add(gAlienTexture)
        .add<TransformComp>(float3(0.f, 0.f, 21.6f), float3(-90.f, 90.f, 0.f).radians(), 0.6f);

    gCamera = world.entity<CameraEntity>("camera")
        .add<OrthoCameraComp>(float3(14.f, -10.f, 10.6f), (kWorldForward * 90.f).radians());

    world.entity("grid")
        .add(gGridMesh).add(gGridTexture)
        // scale is non-uniform to emulate original the vic20 display being non-square
        .add<TransformComp>(float3(0.f, 1.f, 0.f), float3(-90.f, 90.f, 0.f).radians(), float3(0.7f, 0.6f, 0.7f)); 
}

constexpr float2 kTileSize = { 1.4f, 1.2f };
constexpr float2 kWorldBounds = float2(30.f, 21.f);

// use special bounds for the bullet to account for the edges
static bool isBulletInBounds(float2 pos) {
    return pos.x >= -0.5f && pos.x <= kWorldBounds.x
        && pos.y >= -0.5f && pos.y <= kWorldBounds.y + 1.f;
}

static float nudgeToGrid(float z) {
    // round to the nearest kTileSize.y

    float zp = std::round(z / kTileSize.y) * kTileSize.y;

    return zp;
}

static constexpr auto kMovementPatterns = std::to_array<float2>({
    float2(-1, 0), float2(1, 0), float2(0, 1),
    float2(-1, 1), float2(1, 1), float2(0, 1),
    float2(-1, 1), float2(1, 1), float2(0, 1)
});

std::uniform_real_distribution<float> gEggSpawnDist(0.f, 20.f);
std::uniform_int_distribution<size_t> gMovementPatternDist(0, kMovementPatterns.size() - 1);

static float2 getSwarmMovement() {
    size_t idx = gMovementPatternDist(GameService::getRng());
    return kMovementPatterns[idx];
}

static float distance(float2 a, float2 b) {
    float2 d = a - b;
    return std::sqrt(d.x * d.x + d.y * d.y);
}

static IEntity *getBulletHit(game::World& world, float2 position) {
    for (IEntity *pEntity : world.allWith<TransformComp>()) {
        // bullets cant hit themselves
        if (pEntity->get<ProjectileComp>()) continue;

        if (pEntity->get<SwarmBehaviour>() || pEntity->get<EggBehaviour>()) {
            TransformComp *pTransform = pEntity->get<TransformComp>();
            if (distance(pTransform->position.xz(), position) < 0.7f) {
                return pEntity;
            }
        }
    }

    return nullptr;
}

static IEntity *getAlienHit(float2 position) {
    TransformComp *pTransform = gPlayerEntity->get<TransformComp>();

    if (distance(pTransform->position.xz(), position) < 0.3f) {
        return gPlayerEntity;
    }

    return nullptr;
}

float totalTime = 0.f;
float scoreTicker = 0.f;

bool bScore10Seconds = false;
bool bScore30Seconds = false;
bool bScore60Seconds = false;

game_ui::TextWidget gScoreText = { u8"Score: " };
game_ui::TextWidget gScoreBoard = { u8"00000000" };
game_ui::TextWidget gTimeText = { u8"Time: " };
game_ui::TextWidget gTimeBoard = { u8"" };
game_ui::TextWidget gHealthText = { u8"Health: " };
game_ui::TextWidget gHealthBoard = { u8"" };

static char gScoreBuffer[64] = { 0 };
static char gTimeBuffer[64] = { 0 };
static char gHealthBuffer[64] = { 0 };

static void drawPlayerHealth() {
    // draw SM_PLAYER_ICON for each health point
    snprintf(gHealthBuffer, 32, "%s", "");

    // draw an icon for each missing health point
    for (size_t i = gPlayerHealth; i < 3; i++) {
        strcat(gHealthBuffer, "X");
    }
    // draw icon for each health point
    for (size_t i = 0; i < gPlayerHealth; i++) {
        strcat(gHealthBuffer, "+");
    }
    gHealthBoard.text = (const char8_t*)gHealthBuffer;
}

float timeDead = 0.f;
float deadx = 0.f;

float squareWave(float time, float frequency) {
    if (std::sin(time * frequency * 2 * math::kPi<float>) > 0.f) {
        return 1.f;
    } else {
        return -1.f;
    }
}

static void runGameSystems(game::World& world, float delta) {
    auto& workQueue = GameService::getWorkQueue();

    // update the score, always has a base of 10 and 8 zeros
    snprintf(gScoreBuffer, 32, "%010d", gScore.load());
    gScoreBoard.text = (const char8_t*)gScoreBuffer;

    // update time, show minutes, seconds, and milliseconds
    int minutes = (int)totalTime / 60;
    int seconds = (int)totalTime % 60;
    int milliseconds = (int)(totalTime * 1000) % 1000;

    snprintf(gTimeBuffer, 32, "%02dm:%02ds:%02d", minutes, seconds, milliseconds);
    gTimeBoard.text = (const char8_t*)gTimeBuffer;

    drawPlayerHealth();

    scoreTicker += delta;
    totalTime += delta;
    if (scoreTicker > 1.f) {
        scoreTicker = 0.f;
        if (bScore60Seconds) {
            gScore += 50;
        } else if (bScore30Seconds) {
            gScore += 25;
        } else if (bScore10Seconds) {
            gScore += 10;
        }

        gScore += 10;
    }

    if (totalTime > 60.f) {
        bScore60Seconds = true;
    } else if (totalTime > 30.f) {
        bScore30Seconds = true;
    } else if (totalTime > 10.f) {
        bScore10Seconds = true;
    }

    // do movement and shooting input
    for (IEntity *pEntity : world.allWith<PlayerInputComp, ShootComp, TransformComp>()) {
        PlayerInputComp *pInput = pEntity->get<PlayerInputComp>();
        TransformComp *pTransform = pEntity->get<TransformComp>();
        ShootComp *pShoot = pEntity->get<ShootComp>();

        if (gPlayerHealth == 0) {
            timeDead += delta;
            // move back and forth in a square wave pattern for 3 seconds when dead

            if (timeDead < 3.f) {
                float x = squareWave(timeDead, 4.f) * 0.2f;
                pTransform->position.x = deadx + x;

                // spin the player
                pTransform->rotation.z += squareWave(timeDead, 2.f) * 90.f * math::kDegToRad<float>;

                // scale to 0 over 3 seconds (initial scale is 0.6 for reasons)
                pTransform->scale = 0.6f * (1.f - (timeDead / 3.f));
            } else {
                gScene = eScoreScene;
            }
            break;
        }

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

                pShoot->pSound->playSound();

                workQueue.add("bullet", [playerAngle, pTransform, pEntity, speed = pShoot->bulletSpeed] {
                    float2 direction = float2(std::cos(playerAngle), std::sin(playerAngle));

                    World *pWorld = pEntity->getWorld();

                    pWorld->entity("bullet")
                        .add(gBulletMesh).add(gBulletTexture)
                        .add<TransformComp>(pTransform->position, pTransform->rotation, 0.2f)
                        .add<ProjectileComp>(direction * speed);
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

        if (pBehaviour->lastSpawn > pBehaviour->spawnDelay) {
            pBehaviour->lastSpawn = 0.f;

            float x = pTransform->position.x;
            float height = nudgeToGrid(gEggSpawnDist(GameService::getRng()));

            float3 pos = { x, 0.f, height };

            gEggSpawnSound->playSound();

            workQueue.add("egg", [pEntity, pTransform, pos] {
                World *pWorld = pEntity->getWorld();

                gCurrentAliveEggs += 1;
                pWorld->entity("egg")
                    .add<HealthComp>(1, 1, nullptr, gEggDeathSound)
                    .add<TransformComp>(pos, pTransform->rotation, 0.6f)
                    .add(gEggSmallMesh).add(gAlienTexture)
                    .add<EggBehaviour>(1.f, 3.f, 4.5f);
            });
        }
    }

    // hatch eggs
    for (IEntity *pEntity : world.allWith<EggBehaviour, TransformComp>()) {
        EggBehaviour *pBehaviour = pEntity->get<EggBehaviour>();
        TransformComp *pTransform = pEntity->get<TransformComp>();

        pBehaviour->currentTimeAlive += delta;
        if (pBehaviour->currentTimeAlive >= pBehaviour->timeToHatch) {
            gEggHatchSound->playSound();

            workQueue.add("hatch", [pEntity, pTransform] {
                World *pWorld = pEntity->getWorld();

                gCurrentAliveSwarm += 1;
                pWorld->entity("swarmer")
                    .add<SwarmBehaviour>(getSwarmMovement(), 0.3f)
                    .add<HealthComp>(1, 1, nullptr, gAlienDeathSound)
                    .add(pTransform).add(gAlienTexture).add(gAlienMesh);

                pWorld->destroy(pEntity);
            });
        } else if (pBehaviour->currentTimeAlive >= pBehaviour->timeToGrowLarge) {
            if (pBehaviour->state != eEggLarge) {
                pEntity->addComponent(gEggLargeMesh);
                pBehaviour->state = eEggLarge;
                gEggGrowLargeSound->playSound();
            }
        } else if (pBehaviour->currentTimeAlive >= pBehaviour->timeToGrowMedium) {
            if (pBehaviour->state != eEggMedium) {
                pEntity->addComponent(gEggMediumMesh);
                pBehaviour->state = eEggMedium;
                gEggGrowMediumSound->playSound();
            }
        }
    }

    // move swarmers
    for (IEntity *pEntity : world.allWith<SwarmBehaviour, TransformComp>()) {
        SwarmBehaviour *pBehaviour = pEntity->get<SwarmBehaviour>();
        TransformComp *pTransform = pEntity->get<TransformComp>();

        pBehaviour->lastMove += delta;
        if (pBehaviour->lastMove < pBehaviour->timeToMove) continue;

        pBehaviour->lastMove = 0.f;

        if (pTransform->position.x <= 1.f) {
            pBehaviour->direction.x = 1.f;
        } else if (pTransform->position.x >= kWorldBounds.x - 1.f) {
            pBehaviour->direction.x = -1.f;
        }

        if (pTransform->position.z <= 1.f) {
            pBehaviour->direction.y = 1.f;
        } else if (pTransform->position.z >= kWorldBounds.y - 1.f) {
            pBehaviour->direction.y = -1.f;
        }

        pTransform->position.x += pBehaviour->direction.x * kTileSize.x;
        pTransform->position.z += pBehaviour->direction.y * kTileSize.y;
    }

    // check for bullet hits
    for (IEntity *pEntity : world.allWith<ProjectileComp, TransformComp>()) {
        TransformComp *pTransform = pEntity->get<TransformComp>();
        float2 pos = pTransform->position.xz();

        if (IEntity *pHit = getBulletHit(world, pos)) {
            if (HealthComp *pHealth = pHit->get<HealthComp>()) {
                pHealth->takeHit();
            }

            if (EggBehaviour *pEgg = pHit->get<EggBehaviour>()) {
                gCurrentAliveEggs -= 1;
                gScore += (pEgg->state * 50);
            } else {
                gCurrentAliveSwarm -= 1;
                gScore += 250;
            }

            workQueue.add("delete", [pEntity] { 
                World *pWorld = pEntity->getWorld();
                pWorld->destroy(pEntity);
            });
        }
    }

    // did a swarmer hit the player
    for (IEntity *pEntity : world.allWith<SwarmBehaviour, TransformComp>()) {
        TransformComp *pTransform = pEntity->get<TransformComp>();

        if (IEntity *pHit = getAlienHit(pTransform->position.xz())) {
            if (HealthComp *pHealth = pHit->get<HealthComp>()) {
                pHealth->takeHit();
                gPlayerHealth = pHealth->currentHealth;

                if (!pHealth->isAlive()) {
                    gPlayerHealth = 0;
                    deadx = pTransform->position.x;
                    updatePlayingMusic(0.f);
                }
            }

            workQueue.add("delete", [pEntity] { 
                World *pWorld = pEntity->getWorld();
                pWorld->destroy(pEntity);
            });
        }
    }

    // if anything is dead remove it
    for (IEntity *pEntity : world.allWith<HealthComp>()) {
        HealthComp *pHealth = pEntity->get<HealthComp>();

        if (!pHealth->isAlive() && !pHealth->bIsPlayer) {
            workQueue.add("delete", [pEntity] { 
                World *pWorld = pEntity->getWorld();
                pWorld->destroy(pEntity);
            });
        }
    }

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
            float4x4 proj = float4x4::orthographicRH(26.f * aspect, 26.f, 0.1f, 100.f);

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

static void runMenuSystems(game::World& world, float delta) {

}

static void runScoreSystems(game::World& world, float delta) {
    GameService::getScene()
        ->update({ });
}

static void runSystems(game::World& world, float delta) {
    auto& workQueue = GameService::getWorkQueue();
    for (size_t i = 0; i < 16 && workQueue.tryGetMessage(); i++) { }

    mt::WriteLock lock(GameService::getWorldMutex());
    // LOG_INFO("=== update ===");

    switch (gScene) {
    case eGameScene: 
        updatePlayingMusic(delta);
        runGameSystems(world, delta);
        break;
    case eMenuScene:
        runMenuSystems(world, delta);
        break;
    case eScoreScene:
        runScoreSystems(world, delta);
        break;
    }

    // LOG_INFO("=== render ===");
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

    auto *pGraph = RenderService::getGraph();
    const auto& createInfo = pGraph->getCreateInfo();

    game_ui::BoxBounds bounds = {
        .min = 0.f,
        .max = float2(float(createInfo.renderWidth), float(createInfo.renderHeight))
    };

    game_ui::Context layout{bounds};
    game::World& world = GameService::getWorld();
    auto *pHud = GameService::getHud();

    layout.atlas = pHud->pFontAtlas->getInner()->getAtlas();
    layout.shapers.emplace_back(pHud->pFontAtlas->getInner()->getTextShaper(0));

    // score

    gScoreText.align.h = game_ui::AlignH::eLeft;
    gScoreText.align.v = game_ui::AlignV::eTop;
    gScoreBoard.align.h = game_ui::AlignH::eLeft;
    gScoreBoard.align.v = game_ui::AlignV::eTop;

    gScoreText.padding.x = 25.f;

    game_ui::HStackWidget scoreboard;
    scoreboard.add(&gScoreText);
    scoreboard.add(&gScoreBoard);

    // health

    gHealthText.align.h = game_ui::AlignH::eRight;
    gHealthText.align.v = game_ui::AlignV::eBottom;
    gHealthBoard.align.h = game_ui::AlignH::eRight;
    gHealthBoard.align.v = game_ui::AlignV::eBottom;

    gHealthBoard.scale = 4.f;

    // yellow
    gHealthBoard.colour = game_ui::uint8x4(0xff, 0xff, 0x00, 0xff);
    gHealthText.colour = game_ui::uint8x4(0xff, 0xff, 0x00, 0xff);

    game_ui::HStackWidget healthboard;
    healthboard.add(&gHealthText);
    healthboard.add(&gHealthBoard);

    game_ui::HStackWidget gameui;
    gameui.add(&scoreboard);
    gameui.add(&healthboard);

    initEntities(world);

    Clock clock;
    float last = 0.f;

    while (bRunning) {
        ThreadService::pollMain();

        layout.begin(&gameui);

        pHud->update(layout);

        float delta = clock.now() - last;
        last = clock.now();
        runSystems(world, delta);
        std::this_thread::sleep_for(15ms);
    }
}

static int serviceWrapper() try {
    LoggingService::addSink(EditorService::addDebugService<editor_ui::LoggingUi>());

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
