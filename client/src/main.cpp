// core
#include "engine/engine.h"

#include "engine/service/debug.h"
#include "engine/service/platform.h"

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
    simcoe::logInfo("main");

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
        PlatformService::service()
    });
    ServiceRuntime runtime{engineServices};

    // dont use a Region here because we dont want to print `shutdown` if an exception is thrown
    simcoe::logInfo("startup");
    commonMain();
    simcoe::logInfo("shutdown");

    return 0;
} catch (const std::exception& err) {
    simcoe::logError("unhandled exception: {}", err.what());
    return 99;
} catch (...) {
    simcoe::logError("unhandled exception");
    return 99;
}

// gui entry point

int wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    PlatformService::setup(hInstance, nCmdShow);
    return innerMain();
}

// command line entry point

int main(int argc, const char **argv) {
    PlatformService::setup(GetModuleHandle(nullptr), SW_SHOWDEFAULT);
    return innerMain();
}
