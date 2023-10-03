#include "engine/engine.h"
#include "engine/os/system.h"

#include "editor/render/render.h"
#include "editor/render/graph.h"

#include "editor/graph/assets.h"
#include "editor/graph/post.h"
#include "editor/graph/scene.h"

#include <array>

using namespace simcoe;
using namespace editor;

#define ASSERT(expr) \
    do { \
        if (!(expr)) { \
            simcoe::logError(#expr); \
            std::abort(); \
        } \
    } while (false)
    
static constexpr auto kWindowWidth = 1920;
static constexpr auto kWindowHeight = 1080;

static simcoe::System *pSystem = nullptr;
static std::jthread *pRenderThread = nullptr;

struct GameWindow final : IWindowCallbacks {
    void onClose() override {
        delete pRenderThread;
        pSystem->quit();
    }

    void onResize(int width, int height) override {
        logInfo("resize: {}x{}", width, height);
    }
};

static GameWindow gWindowCallbacks;

///
/// entry point
///

static void commonMain() {
    assets::Assets depot = { std::filesystem::current_path() / "build" / "editor.exe.p" };

    const simcoe::WindowCreateInfo windowCreateInfo = {
        .title = "simcoe",
        .style = simcoe::WindowStyle::eWindowed,

        .width = kWindowWidth,
        .height = kWindowHeight,

        .pCallbacks = &gWindowCallbacks
    };

    simcoe::Window window = pSystem->createWindow(windowCreateInfo);
    auto [width, height] = window.getSize().as<UINT>(); // if opened in windowed mode the client size will be smaller than the window size

    const editor::RenderCreateInfo renderCreateInfo = {
        .hWindow = window.getHandle(),
        .depot = depot,

        .displayWidth = width,
        .displayHeight = height,

        .renderWidth = 1920 * 2,
        .renderHeight = 1080 * 2
    };

    // move the render context into the render thread to prevent hangs on shutdown
    std::unique_ptr<editor::RenderContext> context{editor::RenderContext::create(renderCreateInfo)};
    pRenderThread = new std::jthread([ctx = std::move(context)](std::stop_token token) {
        simcoe::Region region("render thread started", "render thread stopped");
        
        RenderGraph graph{ctx.get()};
        auto *pSceneTarget = new graph::SceneTargetHandle();
        auto *pTexture = new graph::TextureHandle("uv-coords.png");
        auto *pBackBuffers = new graph::SwapChainHandle();

        graph.addResource(pBackBuffers);
        graph.addResource(pSceneTarget);
        graph.addResource(pTexture);

        graph.addPass(new graph::ScenePass(pSceneTarget, pTexture));
        graph.addPass(new graph::PostPass(pSceneTarget, pBackBuffers));
        graph.addPass(new graph::PresentPass(pBackBuffers));

        // TODO: if the render loop throws an exception, the program will std::terminate
        // we should handle this case and restart the render loop
        while (!token.stop_requested()) {
            graph.execute();
        }
    });

    while (pSystem->getEvent()) {
        pSystem->dispatchEvent();
    }
}

static int innerMain() try {
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
    pSystem = new simcoe::System(hInstance, nCmdShow);
    return innerMain();
}

// command line entry point

int main(int argc, const char **argv) {
    pSystem = new simcoe::System(GetModuleHandle(nullptr), SW_SHOWDEFAULT);
    return innerMain();
}
