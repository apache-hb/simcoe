// core
#include "engine/engine.h"
#include "engine/system/system.h"

using namespace simcoe;

// globals
static system::System *pSystem = nullptr;
static system::Window *pWindow = nullptr;

// helpers
template<typename T>
struct Cleanup {
    Cleanup(T *ptr) : ptr(ptr) { }
    ~Cleanup() { delete ptr; }
    T *ptr;
};

// callbacks

struct GameWindowCallbacks final : system::IWindowCallbacks {
    void onClose() override {
        pSystem->quit();
    }
};

static GameWindowCallbacks gWindowCallbacks;

///
/// entry point
///

static void commonMain() {
    simcoe::logInfo("main");

    const system::WindowCreateInfo createInfo = {
        .title = "Client",
        .style = system::WindowStyle::eWindowed,
        .width = 1280,
        .height = 720,

        .pCallbacks = &gWindowCallbacks
    };

    Cleanup window(pWindow = pSystem->createWindow(createInfo));

    while (pSystem->getEvent()) {
        pSystem->dispatchEvent();
    }
}

static int innerMain(HINSTANCE hInstance, int nCmdShow) try {
    Cleanup system(pSystem = new system::System(hInstance, nCmdShow));
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
    return innerMain(hInstance, nCmdShow);
}

// command line entry point

int main(int argc, const char **argv) {
    return innerMain(GetModuleHandle(nullptr), SW_SHOWDEFAULT);
}
