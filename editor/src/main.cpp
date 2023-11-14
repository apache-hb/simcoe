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

struct GameWindow final : IWindowCallbacks {
    void onClose() override {
        EditorService::shutdown();
        ThreadService::shutdown();
    }

    void onResize(const WindowSize& event) override {
        EditorService::resizeDisplay(event);
    }

    bool onEvent(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override {
        InputService::handleMsg(uMsg, wParam, lParam);
        return graph::IGuiPass::handleMsg(hWnd, uMsg, wParam, lParam);
    }
};

static GameWindow gWindowCallbacks;

static log::Level getEcsLevel(int32_t level) {
    switch (level) {
    case -4: return log::eAssert;
    case -3: return log::eError;
    case -2: return log::eWarn;
    case -1: return log::eInfo;
    case 0: return log::eDebug;
    default: return log::eDebug;
    }
}

///
/// entry point
///

static void commonMain() {
    debug::setThreadName("main");
    EditorService::start();

    // setup game

    // hook our backtrace support into flecs
    ecs_os_init();

    ecs_os_api_t api = ecs_os_get_api();
    api.abort_ = [](void) { SM_NEVER("flecs error"); };
    api.log_ = [](int32_t level, const char *file, int32_t line, const char *msg) {
        LoggingService::sendMessage(getEcsLevel(level), std::format("{}:{}: {}", file, line, msg));
    };

    ecs_os_set_api(&api);

    flecs::world ecs;

    while (PlatformService::waitForEvent() && !EditorService::shouldQuit()) {
        PlatformService::dispatchEvent();
        ThreadService::pollMainQueue();
    }

    PlatformService::quit();
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
