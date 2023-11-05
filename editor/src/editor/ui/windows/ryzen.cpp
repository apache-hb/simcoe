#include "editor/ui/windows/ryzen.h"

#include "engine/core/win32.h"

#include "engine/threads/service.h"

#include "imgui/imgui_internal.h"
#include "implot/implot.h"

#include <shellapi.h>

using namespace simcoe;
using namespace simcoe::threads;

using namespace editor;
using namespace editor::ui;

using namespace std::chrono_literals;

using amd::RyzenMonitorSerivce;

namespace {
    void launchAdminProcess() {
        // TODO: this will be a bit of a mess
        // we need a seperate exe with just the ryzenmonitor service
        // then need to share some memory between the two processes
        // finally the admin process will poll and write to the shared memory
        // which we can then read from the editor process
        // this also requires a global waitable object to signal from the admin to us
        // when new data is available
        // we also then need to be able to signal the admin process to exit
        // once we exit
        ThreadService::enqueueMain("ryzenmonitor-admin-launch", [] {
            LOG_INFO("attempting to launch admin process");
            ShellExecute(
                /* hwnd= */ nullptr,
                /* lpOperation= */ "runas",
                /* lpFile= */ "C:\\Windows\\notepad.exe", // TODO: open the child process
                /* lpParameters= */ nullptr,
                /* lpDirectory= */ nullptr,
                /* nShowCmd= */ SW_SHOWDEFAULT
            );
        });
    }

    void restartAsAdmin() {
        ThreadService::enqueueMain("ryzenmonitor-admin-restart", [] {
            LOG_INFO("attempting to restart as admin");
            ShellExecute(
                /* hwnd= */ nullptr,
                /* lpOperation= */ "runas",
                /* lpFile= */ GetCommandLine(),
                /* lpParameters= */ nullptr,
                /* lpDirectory= */ nullptr,
                /* nShowCmd= */ SW_SHOWDEFAULT
            );

            PlatformService::quit();
        });
    }
}

CoreInfoHistory::CoreInfoHistory() {
    addFrequency(0.f, 0.f);
    addResidency(0.f, 0.f);
}

void CoreInfoHistory::addFrequency(float time, float f) {
    lastFrequency = f;
    frequency.addPoint(time, f);
}

void CoreInfoHistory::addResidency(float time, float r) {
    lastResidency = r;
    residency.addPoint(time, r);
}

RyzenMonitorDebug::RyzenMonitorDebug()
    : ServiceDebug("RyzenMonitor")
{
    if (RyzenMonitorSerivce::getState() & ~eServiceCreated) {
        setServiceError(RyzenMonitorSerivce::getFailureReason());
        return;
    }

    if (const amd::CpuInfo *pCpuInfo = RyzenMonitorSerivce::getCpuInfo()) {
        coreData.resize(pCpuInfo->getCoreCount());
    }

    bInfoDirty = true;

    ThreadService::newJob("ryzenmonitor", 1s, [this] {
        updateCoreInfo();
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

void RyzenMonitorDebug::drawWindow() {
    if (!bOpen) return;

    auto name = getServiceName();
    if (ImGui::Begin(name.data(), &bOpen)) {
        auto err = getServiceError();
        if (!err.empty()) {
            ImGui::Text("Failed to initialize: %s", err.data());
            if (ImGui::Button("Launch subprocess (broken)")) {
                launchAdminProcess();
            }
            ImGui::SameLine();
            if (ImGui::Button("Restart as admin")) {
                restartAsAdmin();
            }
        } else {
            draw();
        }
    }

    ImGui::End();
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
        ImPlot::PlotShaded(fId, freqHistory.xs(), freqHistory.ys(), freqHistory.size(), -INFINITY, ImPlotShadedFlags_None, freqHistory.offset(), ScrollingBuffer::getStride());
        ImPlot::EndPlot();
    }

    if (bShowResidency && ImPlot::BeginPlot(rId, size, flags)) {
        ImPlot::SetupAxes("Time", "Residency (%)", xFlags, yFlags);
        ImPlot::SetupAxisLimits(ImAxis_X1, lastUpdate - history, lastUpdate, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.f, 100.f, ImGuiCond_Always);
        ImPlot::PlotShaded(rId, resHistory.xs(), resHistory.ys(), resHistory.size(), -INFINITY, ImPlotShadedFlags_None, resHistory.offset(), ScrollingBuffer::getStride());
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

    if (ImGui::BeginTable("Cores", core::intCast<int>(cols), flags)) {
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
    float windowWidth = ImGui::GetWindowWidth();
    float cellWidth = 250.f;
    size_t cols = size_t(windowWidth / cellWidth);
    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchSame;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.f, 0.f));

    if (ImGui::BeginTable("Cores", core::intCast<int>(cols), flags)) {
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
        auto cpuName = pCpuInfo->getName();
        auto description = pCpuInfo->getDescription();
        auto vendor = pCpuInfo->getVendor();
        auto role = pCpuInfo->getRole();
        auto className = pCpuInfo->getClassName();
        auto package = pCpuInfo->getPackage();

        ImGui::Text("Name: %s", cpuName.data());
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
        ImGui::Combo("Hover mode", (int*)&hoverMode, kHoverNames.data(), core::intCast<int>(kHoverNames.size()));
        ImGui::SameLine();
        ImGui::Combo("Display mode", (int*)&displayMode, kDisplayNames.data(), core::intCast<int>(kDisplayNames.size()));
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
    if (RyzenMonitorSerivce::updateCpuInfo()) {
        bInfoDirty = true;
        lastUpdate = clock.now();
        updates += 1;
    } else {
        LOG_WARN("Failed to update cpu info");
    }
}
