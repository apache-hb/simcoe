#include "editor/debug/service.h"

using namespace editor;
using namespace editor::debug;

ThreadServiceDebug::ThreadServiceDebug()
    : ServiceDebug("Threads")
{
    if (ThreadService::getState() & ~eServiceCreated) {
        setFailureReason(ThreadService::getFailureReason());
    }

    geometry = ThreadService::getGeometry();
}

void ThreadServiceDebug::draw() {
    if (ImGui::BeginTabBar("packages")) {
        for (size_t i = 0; i < geometry.packages.size(); ++i) {
            char label[32];
            snprintf(label, sizeof(label), "package: %zu", i);
            if (ImGui::BeginTabItem(label)) {
                drawPackage(i);
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
}

void ThreadServiceDebug::drawPackage(uint16_t i) {
    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_RowBg;

    const auto& [mask, threads, cores, clusters] = geometry.packages[i];
    for (auto clusterId : clusters) {
        auto cluster = geometry.clusters[clusterId];
        char label[32];
        snprintf(label, sizeof(label), "cluster: %u", clusterId);
        if (ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen)) {
            uint16_t fastest = getFastestCore(clusterId);

            if (ImGui::BeginTable("##cores", 3, flags)) {
                ImGui::TableSetupColumn("core", ImGuiTableColumnFlags_WidthStretch, 100.f);
                ImGui::TableSetupColumn("schedule", ImGuiTableColumnFlags_WidthStretch, 100.f);
                ImGui::TableSetupColumn("efficiency", ImGuiTableColumnFlags_WidthStretch, 100.f);
                ImGui::TableHeadersRow();

                for (auto coreId : cluster.coreIds) {
                    ImGui::TableNextColumn();
                    auto core = geometry.cores[coreId];

                    if (coreId == fastest) {
                        ImGui::Text("core: %u (fastest core)", coreId);
                    } else {
                        ImGui::Text("core: %u", coreId);
                    }

                    ImGui::TableNextColumn();
                    ImGui::Text("%u", core.schedule);

                    ImGui::TableNextColumn();
                    ImGui::Text("%u", core.efficiency);
                }

                ImGui::EndTable();
            }
        }
    }
}

uint16_t ThreadServiceDebug::getFastestCore(uint16_t cluster) const {
    uint16_t fastest = 0;
    uint8_t bestSchedule = UINT8_MAX;

    for (auto coreId : geometry.clusters[cluster].coreIds) {
        auto core = geometry.cores[coreId];
        if (core.schedule < bestSchedule) {
            bestSchedule = core.schedule;
            fastest = coreId;
        }
    }

    return fastest;
}