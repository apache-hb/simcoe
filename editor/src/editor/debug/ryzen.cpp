#include "editor/debug/service.h"

#include "implot/implot.h"

using namespace editor;
using namespace editor::debug;

using amd::RyzenMonitorSerivce;

RyzenMonitorDebug::RyzenMonitorDebug()
    : ServiceDebug("RyzenMonitor")
{
    if (RyzenMonitorSerivce::getState() & ~eServiceCreated) {
        setFailureReason(RyzenMonitorSerivce::getFailureReason());
    }
}

std::jthread RyzenMonitorDebug::getWorkThread() {
    std::lock_guard guard(monitorLock);
    if (const amd::CpuInfo *pCpuInfo = RyzenMonitorSerivce::getCpuInfo()) {
        coreData.resize(pCpuInfo->getCoreCount());
    }

    for (auto& data : coreData) {
        data.addFrequency(lastUpdate, 0.f);
        data.addResidency(lastUpdate, 0.f);
    }

    bInfoDirty = true;

    return std::jthread([this](auto token) {
        DebugService::setThreadName("ryzen-monitor");
        LOG_INFO("starting ryzen monitor update thread");

        while (!token.stop_requested()) {
            updateCoreInfo();
            std::this_thread::sleep_for(std::chrono::seconds(1)); // amd recommendation
        }
    });
}

void RyzenMonitorDebug::draw() {
    if (ImGui::CollapsingHeader("BIOS", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawBiosInfo();
    }

    if (ImGui::CollapsingHeader("CPU", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawCpuInfo();
    }
}

void RyzenMonitorDebug::drawBiosInfo() {
    if (const amd::BiosInfo *pBiosInfo = RyzenMonitorSerivce::getBiosInfo()) {
        auto vendor = pBiosInfo->getVendor();
        auto version = pBiosInfo->getVersion();
        auto date = pBiosInfo->getDate();

        uint8_t day = uint8_t(date.getDay());
        uint8_t month = uint8_t(date.getMonth());
        uint16_t year = uint16_t(date.getYear());

        ImGui::Text("Vendor: %s", vendor.data());
        ImGui::Text("Version: %s", version.data());
        ImGui::Text("Date: %02d/%02d/%04d", day, month, year);
    } else {
        ImGui::Text("Failed to get bios info");
    }
}

void RyzenMonitorDebug::drawCoreHistory(size_t i, float width, float heightRatio) {
    float history = 10.f;
    const auto& data = coreData[i];
    const auto& freqHistory = data.frequency;
    const auto& resHistory = data.residency;

    ImPlotAxisFlags xFlags = ImPlotAxisFlags_NoTickLabels;

    if (bShowFrequency && ImPlot::BeginPlot("Frequency", ImVec2(0, 0))) {
        ImPlot::SetupAxes("Time", "Frequency (MHz)", xFlags);
        ImPlot::SetupAxisLimits(ImAxis_X1, lastUpdate - history, lastUpdate, ImGuiCond_Always);
        ImPlot::PlotShaded("Frequency", &freqHistory.Data[0].x, &freqHistory.Data[0].y, freqHistory.Data.size(), freqHistory.Offset, 2 * sizeof(float));
        ImPlot::EndPlot();
    }

    if (bShowResidency && ImPlot::BeginPlot("Residency", ImVec2(0, 0))) {
        ImPlot::SetupAxes("Time", "Residency (%)", xFlags);
        ImPlot::SetupAxisLimits(ImAxis_X1, lastUpdate - history, lastUpdate, ImGuiCond_Always);
        ImPlot::PlotShaded("Residency", &resHistory.Data[0].x, &resHistory.Data[0].y, resHistory.Data.size(), resHistory.Offset, 2 * sizeof(float));
        ImPlot::EndPlot();
    }
}

void RyzenMonitorDebug::drawCore(size_t i) {
    const auto& data = coreData[i];

    if (displayMode == eDisplayHistory) {
        float width = (ImGui::GetWindowWidth() / cols) * 0.9f;
        drawCoreHistory(i, width, 0.3f);
    } else if (displayMode == eDisplayCurrent) {
        ImGui::Text("Frequency: %.1f MHz", data.lastFrequency);
        ImGui::Text("Residency: %.1f %%", data.lastResidency);
    }

    if (hoverMode == eHoverNothing) return;

    if (ImGui::IsItemHovered()) {
        if (ImGui::BeginTooltip()) {
            if (hoverMode == eHoverCurrent) {
                ImGui::Text("Frequency: %.1f MHz", data.lastFrequency);
                ImGui::Text("Residency: %.1f %%", data.lastResidency);
            } else {
                float width = ImGui::GetWindowViewport()->WorkSize.x * 0.6f;
                drawCoreHistory(i, width, 0.1f);
            }
            ImGui::EndTooltip();
        }
    }
}

void RyzenMonitorDebug::drawCpuInfo() {
    if (const amd::CpuInfo *pCpuInfo = RyzenMonitorSerivce::getCpuInfo()) {
        auto name = pCpuInfo->getName();
        auto description = pCpuInfo->getDescription();
        auto vendor = pCpuInfo->getVendor();
        auto role = pCpuInfo->getRole();
        auto className = pCpuInfo->getClassName();
        auto package = pCpuInfo->getPackage();

        ImGui::Text("Name: %s", name.data());
        ImGui::Text("Description: %s", description.data());
        ImGui::Text("Vendor: %s", vendor.data());
        ImGui::Text("Role: %s", role.data());
        ImGui::Text("Class: %s", className.data());
        ImGui::Text("Package: %s", package.data());

        unsigned cores = pCpuInfo->getCoreCount();
        unsigned parked = pCpuInfo->getCorePark();
        ImGui::Text("Cores: %u (parked: %u)", cores, parked);

        if (bInfoDirty && monitorLock.try_lock()) {
            mode = pCpuInfo->getMode();
            socData = pCpuInfo->getSocData();
            const auto& coreInfo = pCpuInfo->getCoreData();

            for (size_t i = 0; i < coreInfo.size(); i++) {
                auto& info = coreInfo[i];
                auto& history = coreData[i];

                history.addFrequency(lastUpdate, info.frequency);
                history.addResidency(lastUpdate, info.residency);
            }

            bInfoDirty = false;
            monitorLock.unlock();
        }

        ImGui::Text("Overclock mode: %s", amd::toString(mode).data());

        ImGui::SeparatorText("SOC info");
        ImGui::Text("Voltage: %.1f A", socData.voltage);

        ImGui::SeparatorText("Core info");

        ImGui::PushItemWidth(100.f);
        ImGui::Combo("Hover mode", (int*)&hoverMode, kHoverNames.data(), kHoverNames.size());
        ImGui::SameLine();
        ImGui::Combo("Display mode", (int*)&displayMode, kDisplayNames.data(), kDisplayNames.size());
        ImGui::PopItemWidth();

        ImGui::Checkbox("Show frequency graphs", &bShowFrequency);
        ImGui::SameLine();
        ImGui::Checkbox("Show residency graphs", &bShowResidency);

        size_t len = coreData.size();
        if (ImGui::BeginTable("Cores", cols, ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders)) {
            for (size_t i = 0; i < len; i++) {
                char label[32];
                snprintf(label, sizeof(label), "Core %zu", i);
                ImGui::TableNextColumn();
                ImGui::Selectable(label);
                drawCore(i);
            }
            ImGui::EndTable();
        }
    } else {
        ImGui::Text("Failed to get cpu info");
    }
}

void RyzenMonitorDebug::updateCoreInfo() {
    std::lock_guard guard(monitorLock);
    RyzenMonitorSerivce::updateCpuInfo();
    bInfoDirty = true;
    lastUpdate = clock.now();
}
