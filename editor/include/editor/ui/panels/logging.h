#pragma once

#include "editor/ui/service.h"
#include "editor/ui/components/buffer.h"

namespace editor::ui {
    struct Message {
        Message(const log::Message& msg);

        bool filter(ImGuiTextFilter& filter) const;
        void draw() const;

        bool repeat(std::string_view msg) {
            if (msg == text) {
                repititions += 1;
                return true;
            }
            return false;
        }

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

        int repititions = 1;
    };

    struct LoggingUi final : ServiceUi, log::ISink {
        LoggingUi();

        void draw() override;

        void accept(const log::Message& msg) override;

    private:
        void clear();
        void drawTable();

        ImGuiTextFilter textFilter;
        bool bAutoScroll = true;  // Keep scrolling if already at the bottom.

        mt::SharedMutex mutex{"LoggingUi"};
        std::vector<Message> messages;
    };
}
