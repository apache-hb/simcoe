#include "editor/ui/panels/logging.h"

#include "engine/core/units.h"
#include "engine/threads/service.h"

using namespace simcoe;

using namespace editor;
using namespace editor::ui;

namespace chrono = std::chrono;

Message::Message(const log::Message& msg)
    : threadId(msg.threadId)
    , level(msg.level)
    , text(msg.msg)
{
    auto ms = chrono::duration_cast<chrono::milliseconds>(msg.time.time_since_epoch()).count() % 1000;
    timestamp = std::format("{:%X}.{:<3}", msg.time, ms);

    bIsMultiline = text.find_first_of("\r\n") != std::string::npos;
}

bool Message::filter(ImGuiTextFilter& filter) const {
    return filter.PassFilter(text.c_str());
}

void Message::draw() const {
    // this is all done inside a table, so we don't need to worry about spacing

    ImGui::TableNextRow(); // but we do need to advance the row

    ImU32 bgColour = bIsMultiline ? ImGui::GetColorU32(ImGuiCol_TableRowBgAlt) : ImGui::GetColorU32(ImGuiCol_TableRowBg);
    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, bgColour);
    ImGui::TableNextColumn();
    ImGui::Text("%s", timestamp.c_str());
    ImGui::TableNextColumn();
    if (auto tid = threads::getThreadName(threadId); tid.empty()) {
        ImGui::Text("0x%lx", threadId);
    } else {
        ImGui::Text("%s", tid.data());
    }
    ImGui::TableNextColumn();
    ImGui::Text("%s", log::toString(level).data());
    ImGui::TableNextColumn();
    if (repititions > 1) {
        ImGui::Text("%s (x %d)", text.c_str(), repititions);
    } else {
        ImGui::Text("%s", text.c_str());
    }
}

LoggingUi::LoggingUi()
    : ServiceUi("Logs")
{
    clear();
}

void LoggingUi::draw() {
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
    textFilter.Draw("Filter", -1.f);

    ImGui::Separator();

    if (bClear) clear();

    drawTable();
}

void LoggingUi::drawTable() {
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
        for (const auto& msg : messages) {
            if (!msg.filter(textFilter)) continue;

            msg.draw();
        }

        // TODO: new messages dont appear for some reason

        if (bAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndTable();
    }
}

void LoggingUi::accept(const log::Message& msg) {
    mt::write_lock lock(mutex);

    if (!messages.empty() && messages.back().repeat(msg.msg)) return;

    messages.emplace_back(msg);
}

void LoggingUi::clear() {
    mt::write_lock lock(mutex);
    messages.clear();
}
