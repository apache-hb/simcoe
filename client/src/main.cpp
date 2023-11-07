// core
#include "engine/core/error.h"
#include "engine/debug/service.h"
#include "engine/service/platform.h"
#include "engine/log/service.h"

using namespace simcoe;

// callbacks

struct GameWindowCallbacks final : IWindowCallbacks {
    void onClose() override {
        PlatformService::quit();
    }
};

static GameWindowCallbacks gWindowCallbacks;

///
/// entry point
///

static void commonMain() {
    LOG_INFO("main");

    PlatformService::showWindow();

    while (PlatformService::getEvent()) {
        PlatformService::dispatchEvent();
    }
}

static int innerMain() try {
    auto engineServices = std::to_array({
        DebugService::service(),
        LoggingService::service(),
        PlatformService::service()
    });
    ServiceRuntime runtime{engineServices, "client"};

    // dont use a Region here because we dont want to print `shutdown` if an exception is thrown
    LOG_INFO("startup");
    commonMain();
    LOG_INFO("shutdown");

    return 0;
} catch (const core::Error& err) {
    LOG_ERROR("unhandled exception: {}", err.what());
    return 99;
} catch (...) {
    LOG_ERROR("unhandled exception");
    return 99;
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
