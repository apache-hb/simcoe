#include "editor/debug/service.h"

#include "imgui/imgui_internal.h"
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
    ImGui::Text("Updates: %zu", updates);

    if (ImGui::CollapsingHeader("BIOS", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawBiosInfo();
    }

    if (ImGui::CollapsingHeader("CPU", ImGuiTreeNodeFlags_DefaultOpen)) {
        drawCpuInfo();
    }

    drawPackageInfo();
    drawSocInfo();
    drawCoreInfo();
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

        auto memInfo = pBiosInfo->getMemoryData();

        if (memInfo.vddioVoltage == UINT16_MAX) {
            ImGui::Text("VDDIO Voltage: N/A");
        } else {
            ImGui::Text("VDDIO Voltage: %d mV", memInfo.vddioVoltage);
        }

        if (memInfo.memClock == UINT16_MAX) {
            ImGui::Text("Memory Clock: N/A");
        } else {
            ImGui::Text("Memory Clock: %d MHz", memInfo.memClock);
        }

        ImGui::Text("CAS CL %u-%u-%u-%u", memInfo.ctrlTcl, memInfo.ctrlTrcdrd, memInfo.ctrlTras, memInfo.ctrlTrp);

    } else {
        ImGui::Text("Failed to get bios info");
    }
}

void RyzenMonitorDebug::drawCoreHistory(size_t i, float width, float heightRatio, bool bHover) {
    float history = 10.f;
    const auto& data = coreData[i];
    const auto& freqHistory = data.frequency;
    const auto& resHistory = data.residency;

    if (!bShowFrequency || !bShowResidency) {
        heightRatio *= 2.f;
    }

    ImVec2 size(width, width * heightRatio);

    ImPlotFlags flags = ImPlotFlags_CanvasOnly;
    ImPlotAxisFlags xFlags = ImPlotAxisFlags_NoDecorations,
                    yFlags = ImPlotAxisFlags_NoDecorations;

    if (bHover) {
        flags &= ~ImPlotFlags_NoLegend;
    }

    const char *fId = "Frequency";
    const char *rId = "Residency";

    ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0.f, 0.f));

    if (bShowFrequency && ImPlot::BeginPlot(fId, size, flags)) {
        ImPlot::SetupAxes("Time", "Frequency (MHz)", xFlags, yFlags);
        ImPlot::SetupAxisLimits(ImAxis_X1, lastUpdate - history, lastUpdate, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.f, 6000.f, ImGuiCond_Always);
        ImPlot::PlotShaded(fId, &freqHistory.Data[0].x, &freqHistory.Data[0].y, freqHistory.Data.size(), -INFINITY, ImPlotShadedFlags_None, freqHistory.Offset, 2 * sizeof(float));
        ImPlot::EndPlot();
    }

    if (bShowResidency && ImPlot::BeginPlot(rId, size, flags)) {
        ImPlot::SetupAxes("Time", "Residency (%)", xFlags, yFlags);
        ImPlot::SetupAxisLimits(ImAxis_X1, lastUpdate - history, lastUpdate, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.f, 100.f, ImGuiCond_Always);
        ImPlot::PlotShaded(rId, &resHistory.Data[0].x, &resHistory.Data[0].y, resHistory.Data.size(), -INFINITY, ImPlotShadedFlags_None, resHistory.Offset, 2 * sizeof(float));
        ImPlot::EndPlot();
    }

    ImPlot::PopStyleVar();
}

ImVec4 RyzenMonitorDebug::getUsageColour(float usage) {
    ImVec4 blue = ImVec4(0.f, 0.f, 1.f, 1.f);
    ImVec4 red = ImVec4(1.f, 0.f, 0.f, 1.f);

    // lerp between blue and red

    ImVec4 result{
        std::lerp(blue.x, red.x, usage),
        std::lerp(blue.y, red.y, usage),
        std::lerp(blue.z, red.z, usage),
        0.3f
    };

    return result;
}

namespace {
    bool isCurrentCellHovered() {
        return ImGui::TableGetHoveredColumn() == ImGui::TableGetColumnIndex()
            && ImGui::TableGetHoveredRow() == ImGui::TableGetRowIndex();
    }
}

void RyzenMonitorDebug::drawCoreInfoCurrentData() {
    float width = ImGui::GetWindowWidth();
    float cellWidth = 150.f;
    size_t cols = size_t(width / cellWidth);

    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_RowBg;

    if (ImGui::BeginTable("Cores", cols, flags)) {
        for (size_t i = 0; i < coreData.size(); i++) {
            ImGui::TableNextColumn();

            const auto& data = coreData[i];
            ImGui::Text("Core %zu", i);
            ImGui::Text("Frequency: %.1f MHz", data.lastFrequency);
            ImGui::Text("Residency: %.1f %%", data.lastResidency);

            if (isCurrentCellHovered()) {
                drawCoreHover(i);
            }
        }
    }
    ImGui::EndTable();
}

void RyzenMonitorDebug::drawCoreInfoHistory() {
    float width = ImGui::GetWindowWidth();
    float cellWidth = 250.f;
    size_t cols = size_t(width / cellWidth);
    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchSame;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.f, 0.f));

    if (ImGui::BeginTable("Cores", cols, flags)) {
        for (size_t i = 0; i < coreData.size(); i++) {
            ImGui::TableNextColumn();

            ImGui::Text("Core %zu", i);
            float width = (ImGui::GetWindowWidth() / cols) * 0.9f;
            drawCoreHistory(i, width, 0.4f, false);
            if (isCurrentCellHovered()) {
                drawCoreHover(i);
            }
        }
        ImGui::EndTable();
    }

    ImGui::PopStyleVar();
}

void RyzenMonitorDebug::drawCoreHover(size_t i) {
    if (displayMode == eDisplayHistory) return;
    if (hoverMode == eHoverNothing) return;

    const auto& data = coreData[i];

    if (ImGui::BeginTooltip()) {
        ImGui::Text("Core %zu", i);

        if (hoverMode == eHoverCurrent) {
            ImGui::Text("Frequency: %.1f MHz", data.lastFrequency);
            ImGui::Text("Residency: %.1f %%", data.lastResidency);
        } else {
            drawCoreHistory(i, 300.f, 0.3f, true);
        }
        ImGui::EndTooltip();
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
            packageData = pCpuInfo->getPackageData();
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
    } else {
        ImGui::Text("Failed to get cpu info");
    }
}

void RyzenMonitorDebug::drawPackageInfo() {
    if (ImGui::CollapsingHeader("Package info")) {
        ImGui::Text("Overclock mode: %s", amd::toString(packageData.mode).data());
        ImGui::Text("Average Core Voltage: %.1f V", packageData.avgCoreVoltage);
        ImGui::Text("Peak Core Voltage: %.1f V", packageData.peakCoreVoltage);

        ImGui::Text("Core Temperature: %.1f C", packageData.temperature);

        ImGui::Text("Peak Speed: %.1f MHz", packageData.peakSpeed);
        ImGui::Text("Fmax(CPU) Frequency: %.1f MHz", packageData.maxClock);
        ImGui::Text("Fabric Clock Frequency: %.1f MHz", packageData.fabricClock);

        ImGui::Text("cHCT Current Limit %.1f C", packageData.chctCurrentLimit);

        float pptFractionCpu = packageData.pptCurrentValue / packageData.pptCurrentLimit;
        ImGui::Text("PPT Current: %.1f W / %.1f W (%.1f %%)", packageData.pptCurrentValue, packageData.pptCurrentLimit, pptFractionCpu * 100.f);
        ImGui::ProgressBar(pptFractionCpu, ImVec2(200.f, 0.f), "PPT Current");

        float tdcFractionCpu = packageData.tdcCurrentValue / packageData.tdcCurrentLimit;
        ImGui::Text("TDC Current: %.1f A / %.1f A (%.1f %%)", packageData.tdcCurrentValue, packageData.tdcCurrentLimit, tdcFractionCpu * 100.f);
        ImGui::ProgressBar(tdcFractionCpu, ImVec2(200.f, 0.f), "TDC Current");

        float edcFractionCpu = packageData.edcCurrentValue / packageData.edcCurrentLimit;
        ImGui::Text("EDC Current: %.1f A / %.1f A (%.1f %%)", packageData.edcCurrentValue, packageData.edcCurrentLimit, edcFractionCpu * 100.f);
        ImGui::ProgressBar(edcFractionCpu, ImVec2(200.f, 0.f), "EDC Current");
    }
}

void RyzenMonitorDebug::drawSocInfo() {
    if (ImGui::CollapsingHeader("SOC info")) {
        ImGui::Text("Voltage: %.1f A", socData.voltage);

        if (socData.edcCurrentValue == -1 || socData.edcCurrentLimit == -1) {
            ImGui::Text("EDC (SOC) Current: N/A");
        } else {
            float edcFraction = socData.edcCurrentValue / socData.edcCurrentLimit;
            ImGui::Text("EDC (SOC) Current: %.1f A / %.1f A (%.1f %%)", socData.edcCurrentValue, socData.edcCurrentLimit, edcFraction * 100.f);
            ImGui::ProgressBar(edcFraction, ImVec2(200.f, 0.f), "EDC (SOC) Current");
        }

        if (socData.tdcCurrentValue == -1 || socData.tdcCurrentLimit == -1) {
            ImGui::Text("TDC (SOC) Current: N/A");
        } else {
            float tdcFraction = socData.tdcCurrentValue / socData.tdcCurrentLimit;
            ImGui::Text("TDC (SOC) Current: %.1f A / %.1f A (%.1f %%)", socData.tdcCurrentValue, socData.tdcCurrentLimit, tdcFraction * 100.f);
            ImGui::ProgressBar(tdcFraction, ImVec2(200.f, 0.f), "TDC (SOC) Current");
        }

        ImGui::Text("VDDCR(VDD) Power: %.1f W", socData.vddcrVddCurrent);
        ImGui::Text("VDDCR(SOC) Power: %.1f W", socData.vddcrSocCurrent);
    }
}

void RyzenMonitorDebug::drawCoreInfo() {
    if (ImGui::CollapsingHeader("Core info")) {
        ImGui::PushItemWidth(100.f);
        ImGui::Combo("Hover mode", (int*)&hoverMode, kHoverNames.data(), kHoverNames.size());
        ImGui::SameLine();
        ImGui::Combo("Display mode", (int*)&displayMode, kDisplayNames.data(), kDisplayNames.size());
        ImGui::PopItemWidth();

        ImGui::Checkbox("Show frequency graphs", &bShowFrequency);
        ImGui::SameLine();
        ImGui::Checkbox("Show residency graphs", &bShowResidency);

        switch (displayMode) {
        case eDisplayCurrent: drawCoreInfoCurrentData(); break;
        case eDisplayHistory: drawCoreInfoHistory(); break;
        default: break;
        }
    }
}

void RyzenMonitorDebug::updateCoreInfo() {
    std::lock_guard guard(monitorLock);
    RyzenMonitorSerivce::updateCpuInfo();
    bInfoDirty = true;
    lastUpdate = clock.now();
    updates += 1;
}