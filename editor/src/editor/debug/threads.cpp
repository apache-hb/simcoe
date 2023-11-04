#include "editor/debug/service.h"

#include "engine/core/range.h"

#include "imgui/imgui_internal.h"

using namespace editor;
using namespace editor::debug;

namespace {
    const char *getPriorityName(threads::ThreadType priority) {
        switch (priority) {
            case threads::ThreadType::eRealtime: return "realtime";
            case threads::ThreadType::eResponsive: return "responsive";
            case threads::ThreadType::eBackground: return "background";
            case threads::ThreadType::eWorker: return "worker";
            default: return "unknown";
        }
    }
}

ThreadServiceDebug::ThreadServiceDebug()
    : ServiceDebug("Threads")
    , geometry(ThreadService::getGeometry())
{
    if (ThreadService::getState() & ~eServiceCreated) {
        setServiceError(ThreadService::getFailureReason());
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

    ImGui::SeparatorText("scheduler");
    mt::read_lock lock(ThreadService::getPoolLock());
    auto& pool = ThreadService::getPool();
    ImGui::Text("total threads: %zu", pool.size());
    ImGui::Text("worker threads: %zu", ThreadService::getWorkerCount());

    if (ImGui::BeginTable("Threads", 4, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch, 100.f);
        ImGui::TableSetupColumn("id", ImGuiTableColumnFlags_WidthStretch, 100.f);
        ImGui::TableSetupColumn("priority", ImGuiTableColumnFlags_WidthStretch, 100.f);
        ImGui::TableSetupColumn("affinity", ImGuiTableColumnFlags_WidthStretch, 100.f);
        ImGui::TableHeadersRow();

        for (const auto *pThread : pool) {
            auto threadName = ThreadService::getThreadName(pThread->getId());
            ImGui::TableNextColumn();
            ImGui::Text("%s", threadName.data());

            ImGui::TableNextColumn();
            ImGui::Text("%lu", pThread->getId());

            ImGui::TableNextColumn();
            ImGui::Text("%s", getPriorityName(pThread->getType()));

            ImGui::TableNextColumn();
            auto affinity = pThread->getAffinity();
            ImGui::Text("%u %llu", affinity.Group, affinity.Mask);
        }

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ImGui::OpenPopup("thread context menu");
        }

        if (ImGui::BeginPopup("thread context menu")) {
            ImGui::EndPopup();
        }

        ImGui::EndTable();
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
    uint16_t bestSchedule = UINT16_MAX;

    for (threads::CoreIndex index : geometry.getChiplet(cluster).coreIds) {
        const auto& core = geometry.getCore(index);
        if (core.schedule < bestSchedule) {
            bestSchedule = core.schedule;
            fastest = index;
        }
    }

    return fastest;
}