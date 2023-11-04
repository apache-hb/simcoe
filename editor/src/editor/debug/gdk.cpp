#include "editor/debug/service.h"

using namespace editor;
using namespace editor::debug;

using microsoft::GdkService;

GdkDebug::GdkDebug()
    : ServiceDebug("GDK")
{
    if (GdkService::getState() & ~eServiceCreated) {
        setServiceError(GdkService::getFailureReason());
    }
}

void GdkDebug::draw() {
    auto info = GdkService::getAnalyticsInfo();
    auto id = GdkService::getConsoleId();
    const auto& features = GdkService::getFeatures();

    auto [osMajor, osMinor, osBuild, osRevision] = info.osVersion;
    ImGui::Text("os: %u.%u.%u - %u", osMajor, osMinor, osBuild, osRevision);

    auto [hostMajor, hostMinor, hostBuild, hostRevision] = info.hostingOsVersion;
    ImGui::Text("host: %u.%u.%u - %u", hostMajor, hostMinor, hostBuild, hostRevision);

    ImGui::Text("family: %s", info.family);
    ImGui::Text("form: %s", info.form);
    ImGui::Text("id: %s", id.data());

    ImGui::SeparatorText("features");

    if (ImGui::BeginTable("features", 2)) {
        ImGui::TableNextColumn();
        ImGui::Text("name");
        ImGui::TableNextColumn();
        ImGui::Text("enabled");

        for (const auto& [featureName, bEnabled] : features) {
            ImGui::TableNextColumn();
            ImGui::Text("%s", featureName.data());
            ImGui::TableNextColumn();
            ImGui::Text("%s", bEnabled ? "enabled" : "disabled");
        }
        ImGui::EndTable();
    }
}
