#include "editor/graph/gui.h"

#include "engine/depot/service.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx12.h"

using namespace editor;
using namespace editor::graph;

using namespace simcoe;

namespace {
    std::recursive_mutex gImguiLock;

    constexpr ImGuiConfigFlags kConfig = ImGuiConfigFlags_DockingEnable
                                       | ImGuiConfigFlags_NavEnableKeyboard;
                                       //| ImGuiConfigFlags_NavEnableGamepad;
}

IGuiPass::IGuiPass(Graph *pGraph, ResourceWrapper<IRTVHandle> *pHandle)
    : IRenderPass(pGraph, "imgui", eDepDisplaySize)
    , iniPath(DepotService::getAssetPath("imgui.ini").string())
{
    setRenderTargetHandle(pHandle);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= kConfig;
    io.IniFilename = iniPath.c_str();

    LOG_INFO("imgui.ini path: {}", iniPath);

    ImGui::StyleColorsDark();
}

IGuiPass::~IGuiPass() {
    ImGui::DestroyContext();
}

void IGuiPass::create() {
    const auto& createInfo = ctx->getCreateInfo();
    guiUniformIndex = ctx->allocSrvIndex();

    auto *pHeap = ctx->getSrvHeap();

    std::lock_guard guard(gImguiLock);
    ImGui_ImplWin32_Init(createInfo.hWindow);
    ImGui_ImplDX12_Init(ctx->getDevice()->getDevice(),
        createInfo.backBufferCount,
        rhi::getTypeFormat(ctx->getSwapChainFormat()),
        pHeap->pHeap->getHeap(),
        D3D12_CPU_DESCRIPTOR_HANDLE { size_t(pHeap->hostOffset(guiUniformIndex)) },
        D3D12_GPU_DESCRIPTOR_HANDLE { size_t(pHeap->deviceOffset(guiUniformIndex)) }
    );
}

void IGuiPass::destroy() {
    auto *pHeap = ctx->getSrvHeap();

    std::lock_guard guard(gImguiLock);

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();

    pHeap->release(guiUniformIndex);
}

void IGuiPass::execute() {
    rhi::Commands *pCommands = ctx->getDirectCommands();

    std::lock_guard guard(gImguiLock);

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    content();

    ImGui::Render();

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCommands->getCommandList());
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT IGuiPass::handleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    std::lock_guard guard(gImguiLock);
    return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
}
