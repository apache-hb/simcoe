#include "engine/engine.h"
#include "engine/os/system.h"

#include "editor/render/render.h"
#include "editor/render/graph.h"

#include <array>

using namespace simcoe;

#define ASSERT(expr) \
    do { \
        if (!(expr)) { \
            simcoe::logError(#expr); \
            std::abort(); \
        } \
    } while (false)
    
static constexpr auto kWindowWidth = 1920;
static constexpr auto kWindowHeight = 1080;

struct GameWindow final : IWindowCallbacks {
    void onResize(int width, int height) override {
        logInfo("resize: {}x{}", width, height);
    }
};

struct GamePass final : editor::IGraphPass {
    void create(editor::RenderContext *ctx) override { 
        pTimer = new simcoe::Timer();
    }

    void destroy(editor::RenderContext *ctx) override { 
        delete pTimer;
    }

    void execute(editor::RenderContext *ctx) override {
        ctx->render(pTimer->now());
    }

private:
    simcoe::Timer *pTimer;
};

struct GameRenderGraph final : editor::IRenderGraph {
    using Super = editor::IRenderGraph;

    static GameRenderGraph *create(editor::RenderContext *ctx) {
        return new GameRenderGraph(ctx);
    }

    void execute() {
        Super::execute(pGamePass);
    }

private:
    GameRenderGraph(editor::RenderContext *ctx) : Super(ctx) {
        pGamePass = addPass<GamePass>();
    }

    editor::IGraphPass *pGamePass;
};

static GameWindow gWindowCallbacks;

static void commonMain(simcoe::System& system) {
    assets::Assets depot = { std::filesystem::current_path() / "build" / "editor.exe.p" };

    const simcoe::WindowCreateInfo windowCreateInfo = {
        .title = "simcoe",
        .style = simcoe::WindowStyle::eWindowed,

        .width = kWindowWidth,
        .height = kWindowHeight,

        .pCallbacks = &gWindowCallbacks
    };

    simcoe::Window window = system.createWindow(windowCreateInfo);
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
    std::jthread renderThread([ctx = std::move(context)](std::stop_token token) {
        simcoe::Region region("render thread started", "render thread stopped");
        std::unique_ptr<GameRenderGraph> graph{GameRenderGraph::create(ctx.get())};

        // TODO: if the render loop throws an exception, the program will std::terminate
        // we should handle this case and restart the render loop
        while (!token.stop_requested()) {
            graph->execute();
        }
    });

    while (system.getEvent()) {
        system.dispatchEvent();
    }
}

static int innerMain(simcoe::System& system) try {
    // dont use a Region here because we dont want to print `shutdown` if an exception is thrown
    simcoe::logInfo("startup");
    commonMain(system);
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
    simcoe::System system(hInstance, nCmdShow);
    return innerMain(system);
}

// command line entry point

int main(int argc, const char **argv) {
    simcoe::System system(GetModuleHandle(nullptr), SW_SHOWDEFAULT);
    return innerMain(system);
}
