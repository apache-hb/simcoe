// core
#include "editor/graph/mesh.h"
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

// flecs
#pragma warning(push, 0)
#pragma warning(disable: 4127)

#include "flecs.h"

#pragma warning(pop)

using namespace simcoe;
using namespace simcoe::math;

using namespace editor;

using namespace game::graph;

using microsoft::GdkService;
using amd::RyzenMonitorSerivce;
using editor::EditorService;
using game::GameService;

static std::atomic_bool bRunning = true;
static flecs::world world;

struct GameWindow final : IWindowCallbacks {
    void onClose() override {
        bRunning = false;

        ThreadService::enqueueMain("quit", [] { world.quit(); });

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

// tags
struct ActiveScene { };
struct SceneRoot { };

// scenes
struct MenuScene { flecs::entity pipeline; };
struct GameScene { flecs::entity pipeline; };
struct ScoreScene { flecs::entity pipeline; };

// game scene
struct Position : public float2 { using float2::float2; };

struct StaticMesh { 
    std::string name;

    ResourceWrapper<IMeshBufferHandle> *pMeshBuffer;
    PassAttachment<IMeshBufferHandle> *pMeshAttachment;
};

// render
struct SceneRender {
    void submit() {
        pScene->update(std::move(batch));
    }

    void record(SceneAction&& action) {
        batch.actions.emplace_back(std::move(action));
    }

    void reset() {
        batch = {};
    }

    ScenePass *pScene = GameService::getScene();
    CommandBatch batch;
};

void resetScene(flecs::world& ecs) {
    ecs.delete_with(flecs::ChildOf, ecs.entity<SceneRoot>());
}

static void menuScene(flecs::iter& it, size_t, ActiveScene) {
    LOG_INFO(">>> menu scene");

    flecs::world ecs = it.world();
    flecs::entity scene = ecs.component<SceneRoot>();

    resetScene(ecs);

    ecs.entity("Menu").child_of(scene);

    ecs.set_pipeline(ecs.get<MenuScene>()->pipeline);
}

static void gameScene(flecs::iter& it, size_t, ActiveScene) {
    LOG_INFO(">>> game scene");

    flecs::world ecs = it.world();
    flecs::entity scene = ecs.component<SceneRoot>();

    resetScene(ecs);

    ecs.entity("Game").child_of(scene);

    ecs.set_pipeline(ecs.get<GameScene>()->pipeline);
}

static void scoreScene(flecs::iter& it, size_t, ActiveScene) {
    LOG_INFO(">>> score scene");

    flecs::world ecs = it.world();
    flecs::entity scene = ecs.component<SceneRoot>();

    resetScene(ecs);

    ecs.entity("Score").child_of(scene);

    ecs.set_pipeline(ecs.get<ScoreScene>()->pipeline);
}

static void initScenes(flecs::world& ecs) {
    ecs.component<ActiveScene>().add(flecs::Exclusive);

    ecs.component<SceneRoot>("Scene root").add<SceneRender>();

    flecs::entity menu = ecs.pipeline()
        .with(flecs::System)
        .build();

    flecs::entity game = ecs.pipeline()
        .with(flecs::System)
        .build();

    flecs::entity score = ecs.pipeline()
        .with(flecs::System)
        .build();

    menu.set_name("Menu Pipeline");
    game.set_name("Game Pipeline");
    score.set_name("Score Pipeline");

    ecs.set<MenuScene>({ menu });
    ecs.set<GameScene>({ game });
    ecs.set<ScoreScene>({ score });

    ecs.observer<ActiveScene>("Scene change to menu")
        .event(flecs::OnAdd)
        .second<MenuScene>()
        .each(menuScene);

    ecs.observer<ActiveScene>("Scene change to game")
        .event(flecs::OnAdd)
        .second<GameScene>()
        .each(gameScene);

    ecs.observer<ActiveScene>("Scene change to score")
        .event(flecs::OnAdd)
        .second<ScoreScene>()
        .each(scoreScene);
}

static void initSystems(flecs::world& ecs) {
    ecs.system<ActiveScene>("Scene change")
        .kind(flecs::OnSet)
        .each([](flecs::entity e, ActiveScene) {
            LOG_INFO("scene change: {}", e.name());
        });

    ecs.system<StaticMesh>("New mesh added")
        .kind(flecs::OnAdd)
        .each([](flecs::entity e, StaticMesh& mesh) {
            LOG_INFO("new mesh added: {}", mesh.name);

            flecs::world ecs = e.world();
            auto *pScene = ecs.component<SceneRender>().get<SceneRender>();
            SM_UNUSED auto *pGraph = pScene->pScene->getGraph();
        });

    ecs.system("Begin renderpass")
        .kind(flecs::PreStore)
        .iter([](flecs::iter& it) {
            LOG_INFO("begin renderpass");

            flecs::world ecs = it.world();
            auto *pScene = ecs.component<SceneRender>().get_mut<SceneRender>();
            pScene->reset();

            pScene->record([](ScenePass* pScene, Context *pContext) {
                pContext->setGraphicsPipeline(pScene->getPipeline());
            });
        });

    ecs.system<StaticMesh>("Draw meshes")
        .kind(flecs::OnStore)
        .each([](flecs::entity e, const StaticMesh& mesh) {
            LOG_INFO("draw mesh");

            flecs::world ecs = e.world();
            auto *pScene = ecs.component<SceneRender>().get_mut<SceneRender>();

            auto *pTransform = e.get<Position>();
            if (pTransform == nullptr) return;

            pScene->record([pMeshBuffer = mesh.pMeshBuffer](ScenePass*, Context *pContext) {
                auto *pBuffer = pMeshBuffer->getInner();

                pContext->setVertexBuffer(pBuffer->getVertexBuffer());
                pContext->setIndexBuffer(pBuffer->getIndexBuffer());

                pContext->drawIndexed(pBuffer->getIndexCount());
            });
        });

    ecs.system("Submit renderpass")
        .kind(flecs::PostFrame)
        .iter([](flecs::iter& it) {
            LOG_INFO("submit renderpass");

            flecs::world ecs = it.world();
            auto *pScene = ecs.component<SceneRender>().get_mut<SceneRender>();
            pScene->submit();
        });
}

///
/// entry point
///

static void commonMain() {
    debug::setThreadName("main");
    EditorService::start();
    RenderService::start();

    world.import<flecs::monitor>();
    world.import<flecs::metrics>();
    world.import<flecs::alerts>();

    initScenes(world);
    initSystems(world);

    world.add<ActiveScene, MenuScene>();

    ecs_app_set_frame_action([](ecs_world_t *pWorld, const ecs_app_desc_t *pDesc) -> int {
        ThreadService::pollMain();
        return !ecs_progress(pWorld, pDesc->delta_time);
    });

    world.app()
        .enable_rest()
        .enable_monitor()
        .target_fps(3.f)
        .run();
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
