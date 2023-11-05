// core
#include "engine/core/mt.h"

// logging
#include "engine/log/sinks.h"

// services
#include "engine/service/service.h"
#include "engine/service/debug.h"
#include "engine/service/platform.h"
#include "engine/service/freetype.h"
#include "engine/log/service.h"
#include "engine/threads/service.h"
#include "engine/depot/service.h"
#include "engine/input/service.h"

// threads
#include "engine/threads/schedule.h"
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

// imgui
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include "imfiles/imfilebrowser.h"
#include "implot/implot.h"

// editor ui
#include "editor/ui/ui.h"
#include "editor/ui/windows/threads.h"
#include "editor/ui/windows/depot.h"
#include "editor/ui/windows/logging.h"

// vendor
#include "vendor/microsoft/gdk.h"
#include "vendor/amd/ryzen.h"

// game
#include "game/service.h"
#include "game/world.h"
#include "game/render/hud.h"
#include "game/render/scene.h"

using namespace simcoe;
using namespace simcoe::math;

using namespace editor;

using microsoft::GdkService;
using amd::RyzenMonitorSerivce;
using game::GameService;

struct GameWindow final : IWindowCallbacks {
    void onClose() override {
        ThreadService::shutdown();
        GameService::shutdown();
    }

    void onResize(const WindowSize& event) override {
        GameService::resizeDisplay(event);
    }

    bool onEvent(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override {
        InputService::handleMsg(uMsg, wParam, lParam);
        return graph::IGuiPass::handleMsg(hWnd, uMsg, wParam, lParam);
    }
};

static GameWindow gWindowCallbacks;

///
/// entry point
///

static void commonMain() {
    debug::setThreadName("main");
    GameService::start();

    // setup game

    while (PlatformService::waitForEvent() && !GameService::shouldQuit()) {
        PlatformService::dispatchEvent();
        ThreadService::pollMainQueue();
    }

    PlatformService::quit();
}

static int serviceWrapper() try {
    LoggingService::addSink("imgui", GameService::addDebugService<ui::LoggingDebug>());

    auto engineServices = std::to_array({
        DebugService::service(),

        PlatformService::service(),
        LoggingService::service(),
        ThreadService::service(),
        InputService::service(),
        DepotService::service(),
        FreeTypeService::service(),
        GameService::service(),

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
