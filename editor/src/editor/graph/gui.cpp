#include "editor/graph/gui.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx12.h"

using namespace editor;
using namespace editor::graph;

namespace {
    std::recursive_mutex imguiLock;

    constexpr ImGuiConfigFlags kConfig = ImGuiConfigFlags_DockingEnable
                                       | ImGuiConfigFlags_NavEnableKeyboard
                                       | ImGuiConfigFlags_NavEnableGamepad;

    constexpr ImGuiDockNodeFlags kDockFlags = ImGuiDockNodeFlags_PassthruCentralNode;

    constexpr ImGuiWindowFlags kWindowFlags = ImGuiWindowFlags_MenuBar
                                            | ImGuiWindowFlags_NoCollapse
                                            | ImGuiWindowFlags_NoMove
                                            | ImGuiWindowFlags_NoResize
                                            | ImGuiWindowFlags_NoTitleBar
                                            | ImGuiWindowFlags_NoBackground
                                            | ImGuiWindowFlags_NoBringToFrontOnFocus
                                            | ImGuiWindowFlags_NoNavFocus
                                            | ImGuiWindowFlags_NoDocking;

    void drawDock() {
        const auto *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));

        ImGui::Begin("Editor", nullptr, kWindowFlags);

        ImGui::PopStyleVar(3);

        ImGuiID id = ImGui::GetID("EditorDock");
        ImGui::DockSpace(id, ImVec2(0.f, 0.f), kDockFlags);

        if (ImGui::BeginMenuBar()) {
            ImGui::Text("Editor");
            ImGui::Separator();

            if (ImGui::BeginMenu("File")) {
                ImGui::MenuItem("Save");
                ImGui::MenuItem("Open");
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Style")) {
                if (ImGui::MenuItem("Classic")) {
                    ImGui::StyleColorsClassic();
                }

                if (ImGui::MenuItem("Dark")) {
                    ImGui::StyleColorsDark();
                }

                if (ImGui::MenuItem("Light")) {
                    ImGui::StyleColorsLight();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        ImGui::End();
    }
}

IGuiPass::IGuiPass(Graph *ctx, ResourceWrapper<IRTVHandle> *pHandle)
    : IRenderPass(ctx, "imgui", eDepDisplaySize)
    , pHandle(addAttachment(pHandle, rhi::ResourceState::eRenderTarget))
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= kConfig;

    ImGui::StyleColorsDark();
}

IGuiPass::~IGuiPass() {
    ImGui::DestroyContext();
}

void IGuiPass::create() {
    const auto& createInfo = ctx->getCreateInfo();
    guiUniformIndex = ctx->allocSrvIndex();

    auto *pHeap = ctx->getSrvHeap();

    std::lock_guard guard(imguiLock);
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

    std::lock_guard guard(imguiLock);

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();

    pHeap->release(guiUniformIndex);
}

void IGuiPass::execute() {
    auto *pCommands = ctx->getDirectCommands();
    auto *pTarget = pHandle->getInner();

    std::lock_guard guard(imguiLock);

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    drawDock();
    content();

    ImGui::Render();

    ctx->setRenderTarget(pTarget->getRtvIndex());
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCommands->getCommandList());
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT IGuiPass::handleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    std::lock_guard guard(imguiLock);
    return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
}
