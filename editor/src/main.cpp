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
#include "game/world.h"

using namespace simcoe;
using namespace simcoe::math;

using namespace game;

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

struct PlayerEntity : public IEntity { using IEntity::IEntity; };
struct AlienEntity : public IEntity { using IEntity::IEntity; };

struct IAssetComp : public IComponent {
    IAssetComp(ObjectData data, fs::path path)
        : IComponent(data)
        , path(path)
    { }

    fs::path path;
};

struct MeshComp : public IAssetComp { 
    using IAssetComp::IAssetComp;
};

struct TextureComp : public IAssetComp {
    using IAssetComp::IAssetComp;
};

struct TransformComp : public IComponent { 
    TransformComp(ObjectData data, float3 position = 0.f, float3 rotation = 0.f, float3 scale = 1.f)
        : IComponent(data)
        , position(position)
        , rotation(rotation)
        , scale(scale)
    { }

    float3 position;
    float3 rotation;
    float3 scale;
};

static void initEntities(game::World *pWorld) {
    pWorld->create<PlayerEntity>("player")
        .add<MeshComp>("player.model")
        .add<TextureComp>("player.png")
        .add<TransformComp>(0.f, 0.f, 1.f);

    pWorld->create<AlienEntity>("alien")
        .add<MeshComp>("alien.model")
        .add<TextureComp>("alien.png")
        .add<TransformComp>(0.f, 0.f, 1.f);
}

static void runSystems(game::World *pWorld, float delta) {
    LOG_INFO("=== begin game tick ===");

    if (PlayerEntity *pPlayer = pWorld->get<PlayerEntity>()) {
        LOG_INFO("player: {} (delta {})", pPlayer->getName(), delta);
    }

    pWorld->all([](IEntity *pEntity) {
        if (TransformComp *pTransform = pEntity->get<TransformComp>()) {
            LOG_INFO("entity: {} (pos {})", pEntity->getName(), pTransform->position);
        } else {
            LOG_INFO("entity: {}", pEntity->getName());
        }
    });

    LOG_INFO("=== end game tick ===");
}

///
/// entry point
///

using namespace std::chrono_literals;

static void commonMain() {
    debug::setThreadName("main");
    EditorService::start();
    RenderService::start();

    game::World *pWorld = new game::World();

    initEntities(pWorld);

    Clock clock;
    float last = clock.now();

    while (bRunning) {
        ThreadService::pollMain();

        float delta = clock.now() - last;
        last = clock.now();
        runSystems(pWorld, delta);
        std::this_thread::sleep_for(500ms);
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
