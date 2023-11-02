#include "editor/debug/logging.h"

#include "engine/core/units.h"
#include "engine/threads/service.h"

using namespace simcoe;

using namespace editor;
using namespace editor::debug;

namespace chrono = std::chrono;

namespace {
    constexpr const char *getLevelName(LogLevel level) {
        switch (level) {
        case eAssert: return "panic";
        case eError: return "error";
        case eWarn: return "warn";
        case eInfo: return "info";
        case eDebug: return "debug";
        default: return "unknown";
        }
    }
}

LoggingDebug::LoggingDebug()
    : ServiceDebug("Logs")
{
    clear();
}

void LoggingDebug::draw() {
    // Options menu
    if (ImGui::BeginPopup("Options")) {
        ImGui::Checkbox("Auto-scroll", &bAutoScroll);
        ImGui::EndPopup();
    }

    // Main window
    if (ImGui::Button("Options"))
        ImGui::OpenPopup("Options");

    ImGui::SameLine();
    bool bClear = ImGui::Button("Clear");

    ImGui::SameLine();
    textFilter.Draw("Filter", -100.0f);

    ImGui::Separator();

    if (bClear) clear();

    drawTable();

    if (bAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(0.999f);
}

void LoggingDebug::drawTable() {
    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("logs", 4, flags)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("time", ImGuiTableColumnFlags_WidthFixed, 100.f);
        ImGui::TableSetupColumn("thread", ImGuiTableColumnFlags_WidthFixed, 100.f);
        ImGui::TableSetupColumn("level", ImGuiTableColumnFlags_WidthFixed, 100.f);
        ImGui::TableSetupColumn("message", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        // freeze the first row so it's always visible when scrolling

        mt::read_lock lock(mutex);
        ImGuiListClipper clipper;
        clipper.Begin(core::intCast<int>(messages.size()));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                const auto& msg = messages[row];
                if (!textFilter.PassFilter(msg.msg.c_str())) continue;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", msg.time.c_str());
                ImGui::TableNextColumn();
                if (auto tid = ThreadService::getThreadName(msg.threadId); tid.empty()) {
                    ImGui::Text("0x%lx", msg.threadId);
                } else {
                    ImGui::Text("%s", tid.data());
                }
                ImGui::TableNextColumn();
                ImGui::Text("%s", getLevelName(msg.level));
                ImGui::TableNextColumn();
                ImGui::Text("%s", msg.msg.c_str());
            }
        }
        ImGui::EndTable();
    }
}

void LoggingDebug::accept(const LogMessage& msg) {
    auto ms = chrono::duration_cast<chrono::milliseconds>(msg.time.time_since_epoch()).count() % 1000;
    auto time = std::format("{:%X}.{:<3}", msg.time, ms);

    mt::write_lock lock(mutex);
    messages.push_back({
        .time = time,
        .threadId = msg.threadId,
        .level = msg.level,

        .msg = std::string(msg.msg)
    });
}

void LoggingDebug::clear() {
    mt::write_lock lock(mutex);

    messages.clear();
}
