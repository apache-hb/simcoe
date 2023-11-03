// core
#include "engine/service/debug.h"
#include "engine/service/platform.h"
#include "engine/service/logging.h"

using namespace simcoe;

// globals
static Window *pWindow = nullptr;

// helpers
template<typename T>
struct Cleanup {
    Cleanup(T *ptr) : ptr(ptr) { }
    ~Cleanup() { delete ptr; }
    T *ptr;
};

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

    const WindowCreateInfo createInfo = {
        .title = "Client",
        .style = WindowStyle::eWindowed,
        .size = { 1280, 720 },

        .pCallbacks = &gWindowCallbacks
    };

    Cleanup window(pWindow = new Window(createInfo));

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
} catch (const std::exception& err) {
    LOG_ERROR("unhandled exception: {}", err.what());
    return 99;
} catch (...) {
    LOG_ERROR("unhandled exception");
    return 99;
}

// gui entry point

int wWinMain(HINSTANCE hInstance, SM_UNUSED HINSTANCE hPrevInstance, SM_UNUSED LPWSTR lpCmdLine, int nCmdShow) {
    PlatformService::setup(hInstance, nCmdShow);
    return innerMain();
}

// command line entry point

int main(SM_UNUSED int argc, SM_UNUSED const char **argv) {
    PlatformService::setup(GetModuleHandle(nullptr), SW_SHOWDEFAULT);
    return innerMain();
}
