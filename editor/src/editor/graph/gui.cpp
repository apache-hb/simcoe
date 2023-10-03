#include "editor/graph/gui.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx12.h"

using namespace editor;
using namespace editor::graph;

namespace {
    std::recursive_mutex imguiLock;
}

IGuiPass::IGuiPass(IResourceHandle *pHandle)
    : IRenderPass()
    , pHandle(addResource(pHandle, render::ResourceState::eRenderTarget))
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();
}

IGuiPass::~IGuiPass() {
    ImGui::DestroyContext();
}

void IGuiPass::create(RenderContext *ctx) {
    const auto& createInfo = ctx->getCreateInfo();
    guiUniformIndex = ctx->allocSrvIndex();

    auto *pHeap = ctx->getDataAlloc();

    std::lock_guard guard(imguiLock);
    ImGui_ImplWin32_Init(createInfo.hWindow);
    ImGui_ImplDX12_Init(ctx->getDevice()->getDevice(),
        RenderContext::kBackBufferCount,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        pHeap->pHeap->getHeap(),
        D3D12_CPU_DESCRIPTOR_HANDLE { size_t(pHeap->hostOffset(guiUniformIndex)) },
        D3D12_GPU_DESCRIPTOR_HANDLE { size_t(pHeap->deviceOffset(guiUniformIndex)) }
    );
}

void IGuiPass::destroy(RenderContext *ctx) {
    std::lock_guard guard(imguiLock);
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
}

void IGuiPass::execute(RenderContext *ctx) {
    auto *pCommands = ctx->getDirectCommands();
    auto *pTarget = pHandle->getHandle();

    std::lock_guard guard(imguiLock);

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    content();

    ImGui::Render();

    ctx->setRenderTarget(pTarget->getRtvIndex(ctx));
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCommands->getCommandList());
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT IGuiPass::handleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    std::lock_guard guard(imguiLock);
    return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
}