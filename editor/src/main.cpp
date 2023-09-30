#include "engine/engine.h"
#include "engine/os/system.h"

#include "editor/render.h"

#include <array>

using namespace simcoe;

#define ASSERT(expr) \
    do { \
        if (!(expr)) { \
            simcoe::logError(#expr); \
            std::abort(); \
        } \
    } while (false)

static void commonMain(simcoe::System& system) {
    assets::Assets depot = { std::filesystem::current_path() / "build" / "editor.exe.p" };

    const simcoe::WindowCreateInfo windowCreateInfo = {
        .title = "simcoe",
        .style = simcoe::WindowStyle::eWindowed,
        .width = 1920,
        .height = 1080
    };

    simcoe::Window window = system.createWindow(windowCreateInfo);

    const editor::RenderCreateInfo renderCreateInfo = {
        .hWindow = window.getHandle(),
        .depot = depot,

        .displayWidth = 1920,
        .displayHeight = 1080,

        .renderWidth = 800,
        .renderHeight = 600
    };

    auto *pContext = editor::RenderContext::create(renderCreateInfo);

    std::jthread renderThread([&](std::stop_token token) {
        simcoe::Region region("render thread started", "render thread stopped");

        while (!token.stop_requested()) {
            pContext->render();
        }
    });

    while (system.getEvent()) {
        system.dispatchEvent();
    }
}

static int innerMain(simcoe::System& system) try {
    // dont use a Region here because we want to know if commonMain throws an exception
    simcoe::logInfo("startup");
    commonMain(system);
    simcoe::logInfo("shutdown");

    return 0;
} catch (const std::exception& err) {
    simcoe::logError(std::format("unhandled exception: {}", err.what()));
    return 99;
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    simcoe::System system(hInstance, nCmdShow);
    return innerMain(system);
}

int main(int argc, const char **argv) {
    simcoe::System system(GetModuleHandle(nullptr), SW_SHOWDEFAULT);
    return innerMain(system);
}
