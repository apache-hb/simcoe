#pragma once

#include "editor/debug/debug.h"

#include "imgui/imgui.h"

namespace editor::debug {
    struct LoggingDebug final : ServiceDebug, ISink {
        struct Message {
            std::string time;
            threads::ThreadId threadId;
            LogLevel level;

            std::string msg;
        };

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
