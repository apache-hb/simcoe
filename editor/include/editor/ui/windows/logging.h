#pragma once

#include "editor/ui/service.h"
#include "editor/ui/components/buffer.h"

namespace editor::ui {
    struct Message {
        Message(const log::Message& msg);

        bool filter(ImGuiTextFilter& filter) const;
        void draw() const;

    private:
        // time as a string, HH:MM:SS.mmm format
        std::string timestamp;

        // store the threadid rather than the name
        // if the name changes, we get the new one
        threads::ThreadId threadId;
        log::Level level;

        // if the message contains newlines, we put borders above and below it
        bool bIsMultiline;
        std::string text;
    };

    struct LoggingDebug final : ServiceDebug, log::ISink {
        LoggingDebug();

        void draw() override;

        void accept(const log::Message& msg) override;

    private:
        void clear();
        void drawTable();

        ImGuiTextFilter textFilter;
        bool bAutoScroll = true;  // Keep scrolling if already at the bottom.

        mt::shared_mutex mutex;
        std::vector<Message> messages;
    };
}
