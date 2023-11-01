#include "editor/debug/service.h"

#include "engine/core/range.h"

#include "imgui/imgui_internal.h"

using namespace editor;
using namespace editor::debug;

ThreadServiceDebug::ThreadServiceDebug()
    : ServiceDebug("Threads")
    , geometry(ThreadService::getGeometry())
{
    if (ThreadService::getState() & ~eServiceCreated) {
        setFailureReason(ThreadService::getFailureReason());
    }
}

void ThreadServiceDebug::draw() {
    if (ImGui::BeginTabBar("packages")) {
        for (threads::PackageIndex i : core::Range(threads::PackageIndex(geometry.packages.size()))) {
            char label[32];
            snprintf(label, sizeof(label), "package: %u", uint16_t(i));
            if (ImGui::BeginTabItem(label)) {
                drawPackage(i);
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
}

void ThreadServiceDebug::drawPackage(threads::PackageIndex i) {
    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_RowBg;

    const auto& [mask, threads, cores, chiplets] = geometry.getPackage(i);
    for (const auto& [index, cluster] : core::enumerate<threads::ChipletIndex>(chiplets)) {
        char label[32];
        snprintf(label, sizeof(label), "chiplet: %u", uint16_t(index));
        if (ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) {
            threads::CoreIndex fastest = getFastestCore(index);

            if (ImGui::BeginTable("##cores", 3, flags)) {
                ImGui::TableSetupColumn("core", ImGuiTableColumnFlags_WidthStretch, 100.f);
                ImGui::TableSetupColumn("schedule", ImGuiTableColumnFlags_WidthStretch, 100.f);
                ImGui::TableSetupColumn("efficiency", ImGuiTableColumnFlags_WidthStretch, 100.f);
                ImGui::TableHeadersRow();

                for (threads::CoreIndex coreId : geometry.getChiplet(index).coreIds) {
                    ImGui::TableNextColumn();
                    auto core = geometry.getCore(coreId);

                    if (coreId == fastest) {
                        ImGui::Text("core: %u (fastest core)", uint16_t(coreId));
                    } else {
                        ImGui::Text("core: %u", uint16_t(coreId));
                    }

                    ImGui::TableNextColumn();
                    ImGui::Text("%u", core.schedule);

                    ImGui::TableNextColumn();
                    ImGui::Text("%u", core.efficiency);

                    if (ImGui::TableGetHoveredRow() == ImGui::TableGetRowIndex()) {
                        ImGui::BeginTooltip();
                        for (threads::SubcoreIndex subcoreIds : core.subcoreIds) {
                            ImGui::Text("subcore: %u", uint16_t(subcoreIds));
                        }
                        ImGui::EndTooltip();
                    }
                }

                ImGui::EndTable();
            }
        }
    }
}

threads::CoreIndex ThreadServiceDebug::getFastestCore(threads::ChipletIndex cluster) const {
    threads::CoreIndex fastest = threads::CoreIndex::eInvalid;
    uint8_t bestSchedule = UINT8_MAX;

    for (threads::CoreIndex index : geometry.getChiplet(cluster).coreIds) {
        const auto& core = geometry.getCore(index);
        if (core.schedule < bestSchedule) {
            bestSchedule = core.schedule;
            fastest = index;
        }
    }

    return fastest;
}