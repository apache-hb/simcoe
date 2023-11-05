#include "editor/ui/windows/depot.h"

#include "engine/depot/service.h"

using namespace editor;
using namespace editor::ui;

using namespace simcoe;

namespace {
    const char *modeToString(depot::FileMode mode) {
        switch (mode) {
            case depot::FileMode::eRead: return "Read";
            case depot::FileMode::eReadWrite: return "Read/Write";
            default: return "Unknown";
        }
    }
}

DepotDebug::DepotDebug()
    : ServiceDebug("Depot")
{ }

void DepotDebug::draw() {
    mt::read_lock lock(DepotService::getMutex());

    if (ImGui::Button("Open File")) {
        openFile.Open();
    }

    constexpr ImGuiTableFlags flags = ImGuiTableFlags_Borders
                                    | ImGuiTableFlags_RowBg
                                    | ImGuiTableFlags_SizingFixedFit;

    if (ImGui::CollapsingHeader("Depot handles")) {
        if (ImGui::BeginTable("VFS Handles", 3, flags)) {
            ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Handles", ImGuiTableColumnFlags_WidthFixed);

            ImGui::TableHeadersRow();

            for (const auto& [path, handle] : DepotService::getHandles()) {
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::Text("%s", path.string().c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%s", modeToString(handle->getMode()));

                ImGui::TableNextColumn();
                ImGui::Text("%ld", handle.use_count());
            }

            ImGui::EndTable();
        }
    }

    openFile.Display();

    if (openFile.HasSelected()) {
        auto path = openFile.GetSelected();
        LOG_INFO("selected file: {}", path.string());
        openFile.ClearSelected();

        ThreadService::enqueueWork("open", [this, path] {
            files.emplace_back(DepotService::openExternalFile(path));
        });
    }
}
