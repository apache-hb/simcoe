// core
#include "engine/core/mt.h"

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
#include "editor/graph/scene.h"
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

using namespace simcoe;
using namespace simcoe::math;

using namespace editor;
using microsoft::GdkService;
using amd::RyzenMonitorSerivce;
using editor::EditorService;
using game::GameService;

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

static GameWindow gWindowCallbacks;

// game logic

// scene relationships & tags
struct ActiveScene { };
struct SceneRoot { };

// scenes
struct MenuScene { flecs::entity root; };
struct GameScene { flecs::entity root; };
struct ScoreScene { flecs::entity root; };

// game relationships & tags
struct Player { };
struct Bullet { };
struct Enemy { };
struct Egg { };

// game components
struct Transform {
    float3 position;
    float3 rotation;
    float3 scale;
};

struct Health {
    size_t currentHealth;
    size_t maxHealth;
};

void resetScene(flecs::world& ecs) {
    ecs.delete_with(flecs::ChildOf, ecs.entity<SceneRoot>());
}

void doMenuScene(flecs::iter& it, size_t, ActiveScene) {
    auto ecs = it.world();

    auto scene = ecs.entity<SceneRoot>();
    resetScene(ecs);

    ecs.set_pipeline(ecs.get<MenuScene>()->root);
}

void doGameScene(flecs::iter& it, size_t, ActiveScene) {
    auto ecs = it.world();

    auto scene = ecs.entity<SceneRoot>();
    resetScene(ecs);

    ecs.component<Player>();

    ecs.entity("Player")
        .add<Player>()
        .set(Health { 3, 5 })
        .child_of(scene);
    
    ecs.set_pipeline(ecs.get<GameScene>()->root);
}

void doScoreScene(flecs::iter& it, size_t, ActiveScene) {
    auto ecs = it.world();

    auto scene = ecs.entity<SceneRoot>();
    resetScene(ecs);

    ecs.set_pipeline(ecs.get<ScoreScene>()->root);
}

static void initScenes(flecs::world& ecs) {
    ecs.component<ActiveScene>()
        .add(flecs::Exclusive);

    auto menu = ecs.pipeline()
        .with(flecs::System)
        .without<GameScene>().without<ScoreScene>()
        .build();

    auto game = ecs.pipeline()
        .with(flecs::System)
        .without<MenuScene>().without<ScoreScene>()
        .build();

    auto scoreboard = ecs.pipeline()
        .with(flecs::System)
        .without<GameScene>().without<MenuScene>()
        .build();

    ecs.set<MenuScene>({ menu });
    ecs.set<GameScene>({ game });
    ecs.set<ScoreScene>({ scoreboard });

    ecs.observer<ActiveScene>("Scene change to menu")
        .event(flecs::OnAdd)
        .second<MenuScene>()
        .each(doMenuScene);

    ecs.observer<ActiveScene>("Scene change to game")
        .event(flecs::OnAdd)
        .second<GameScene>()
        .each(doGameScene);

    ecs.observer<ActiveScene>("Scene change to scoreboard")
        .event(flecs::OnAdd)
        .second<ScoreScene>()
        .each(doScoreScene);
}

static void initSystems(flecs::world& ecs) {
    ecs.system<Health>("Display player health")
        .kind<GameScene>()
        .each([](flecs::entity entity, Health& health) {
            auto world = entity.world();
            if (health.currentHealth == 0) {
                entity.destruct();
            }

            LOG_INFO("health: {}/{}", health.currentHealth, health.maxHealth);
        });
}

///
/// entry point
///

static void commonMain() {
    debug::setThreadName("main");
    EditorService::start();
    RenderService::start();

    auto& world = GameService::getWorld();
    initScenes(world);
    initSystems(world);

    world.add<ActiveScene, GameScene>();

    // setup game
    while (bRunning) {
        ThreadService::pollMainQueue();
        GameService::progress();
    }
}

static int serviceWrapper() try {
    LoggingService::addSink(EditorService::addDebugService<ui::LoggingUi>());

    auto engineServices = std::to_array({
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
