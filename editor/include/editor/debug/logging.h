#pragma once

#include "editor/debug/debug.h"

#include "imgui/imgui.h"

namespace editor::debug {
    struct Message {
        Message(const LogMessage& msg);

        bool filter(ImGuiTextFilter& filter) const;
        void draw() const;

    private:
        // time as a string, HH:MM:SS.mmm format
        std::string timestamp;

        // store the threadid rather than the name
        // if the name changes, we get the new one
        threads::ThreadId threadId;
        LogLevel level;

        // if the message contains newlines, we put borders above and below it
        bool bIsMultiline;
        std::string text;
    };

    struct LoggingDebug final : ServiceDebug, ISink {
        LoggingDebug();

        void draw() override;

        void accept(const LogMessage& msg) override;

    private:
        void clear();
        void drawTable();

        ImGuiTextFilter textFilter;
        bool bAutoScroll = true;  // Keep scrolling if already at the bottom.

        mt::shared_mutex mutex;
        std::vector<Message> messages;
    };
}
