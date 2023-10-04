#include "engine/engine.h"
#include "engine/os/system.h"

#include "editor/render/render.h"
#include "editor/render/graph.h"

#include "editor/graph/assets.h"
#include "editor/graph/post.h"
#include "editor/graph/scene.h"
#include "editor/graph/gui.h"

#include "imgui/imgui.h"

#include "moodycamel/concurrentqueue.h"

#include <functional>

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
static RenderGraph *pGraph = nullptr;

using WorkItem = std::function<void()>;

moodycamel::ConcurrentQueue<WorkItem> gWorkQueue{64};
static std::jthread *pWorkThread = nullptr;

struct GameWindow final : IWindowCallbacks {
    void onClose() override {
        delete pWorkThread;
        delete pRenderThread;
        pSystem->quit();
    }

    void onResize(int width, int height) override {
        gWorkQueue.enqueue([width, height] {
            if (pGraph != nullptr) pGraph->resizeDisplay(width, height);
            logInfo("resize-display: {}x{}", width, height);
        });
    }

    bool onEvent(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override {
        return graph::IGuiPass::handleMsg(hWnd, uMsg, wParam, lParam);
    }
};

struct GameGui final : graph::IGuiPass {
    using graph::IGuiPass::IGuiPass;

    int renderSize[2];
    int backBufferCount;

    void create(RenderContext *ctx) override {
        IGuiPass::create(ctx);

        const auto &createInfo = ctx->getCreateInfo();
        renderSize[0] = createInfo.renderWidth;
        renderSize[1] = createInfo.renderHeight;

        backBufferCount = createInfo.backBufferCount;
    }

    void content(RenderContext *ctx) override {
        ImGui::ShowDemoWindow();

        ImGui::Begin("debug");
        auto *pSrvHeap = ctx->getSrvHeap();
        auto *pRtvHeap = ctx->getRtvHeap();
        auto& rtvAlloc = pRtvHeap->mem;

        const auto& createInfo = ctx->getCreateInfo();
        ImGui::Text("present: %dx%d", createInfo.displayWidth, createInfo.displayHeight);
        ImGui::Text("render: %dx%d", createInfo.renderWidth, createInfo.renderHeight);

        if (ImGui::SliderInt2("render size", renderSize, 64, 4096)) {
            gWorkQueue.enqueue([this] {
                pGraph->resizeRender(renderSize[0], renderSize[1]);
                logInfo("resize-render: {}x{}", renderSize[0], renderSize[1]);
            });
        }

        if (ImGui::SliderInt("backbuffer count", &backBufferCount, 2, 8)) {
            gWorkQueue.enqueue([this] {
                pGraph->changeBackBufferCount(backBufferCount);
                logInfo("change-backbuffer-count: {}", backBufferCount);
            });
        }

        ImGui::Text("rtv heap: %zu", rtvAlloc.getSize());
        for (size_t i = 0; i < rtvAlloc.getSize(); i++) {
            ImGui::BulletText("%zu: %s", i, rtvAlloc.test(engine::BitMap::Index(i)) ? "used" : "free");
        }

        ImGui::Text("srv heap: %zu", pSrvHeap->mem.getSize());
        for (size_t i = 0; i < pSrvHeap->mem.getSize(); i++) {
            ImGui::BulletText("%zu: %s", i, pSrvHeap->mem.test(engine::BitMap::Index(i)) ? "used" : "free");
        }

        ImGui::End();
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
    auto [realWidth, realHeight] = window.getSize().as<UINT>(); // if opened in windowed mode the client size will be smaller than the window size

    const editor::RenderCreateInfo renderCreateInfo = {
        .hWindow = window.getHandle(),
        .depot = depot,

        .displayWidth = realWidth,
        .displayHeight = realHeight,

        .renderWidth = 1920 * 2,
        .renderHeight = 1080 * 2
    };

    pWorkThread = new std::jthread([](std::stop_token token) {
        while (!token.stop_requested()) {
            WorkItem item;
            if (gWorkQueue.try_dequeue(item)) {
                item();
            }
        }
        simcoe::logInfo("work thread stopped");
    });

    // move the render context into the render thread to prevent hangs on shutdown
    editor::RenderContext *pContext = editor::RenderContext::create(renderCreateInfo);
    pRenderThread = new std::jthread([pContext](std::stop_token token) {
        simcoe::Region region("render thread started", "render thread stopped");

        pGraph = new RenderGraph(pContext);
        auto *pSceneTarget = new graph::SceneTargetHandle();
        auto *pTexture = new graph::TextureHandle("uv-coords.png");
        auto *pUniform = new graph::UniformHandle();
        auto *pBackBuffers = new graph::SwapChainHandle();

        pGraph->addResource(pBackBuffers);
        pGraph->addResource(pSceneTarget);
        pGraph->addResource(pUniform);
        pGraph->addResource(pTexture);

        pGraph->addPass(new graph::ScenePass(pSceneTarget, pTexture, pUniform));
        pGraph->addPass(new graph::PostPass(pSceneTarget, pBackBuffers));
        pGraph->addPass(new GameGui(pBackBuffers));
        pGraph->addPass(new graph::PresentPass(pBackBuffers));

        // TODO: if the render loop throws an exception, the program will std::terminate
        // we should handle this case and restart the render loop
        while (!token.stop_requested()) {
            pGraph->execute();
        }

        delete pGraph;
        delete pContext;
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
