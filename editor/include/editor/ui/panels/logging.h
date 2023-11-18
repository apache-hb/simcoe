#pragma once

#include "editor/ui/service.h"
#include "editor/ui/components/buffer.h"

namespace editor::ui {
    namespace engine_log = simcoe::log;
    namespace engine_mt = simcoe::mt;
    namespace engine_threads = simcoe::threads;

    struct Message {
        Message(const engine_log::Message& msg);

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
        engine_threads::ThreadId threadId;
        engine_log::Level level;

        // if the message contains newlines, we put borders above and below it
        bool bIsMultiline;
        std::string text;

        int repititions = 1;
    };

    struct LoggingUi final : ServiceUi, engine_log::ISink {
        LoggingUi();

        void draw() override;

        void accept(const engine_log::Message& msg) override;

    private:
        void clear();
        void drawTable();

        ImGuiTextFilter textFilter;
        bool bAutoScroll = true;  // Keep scrolling if already at the bottom.

        engine_mt::SharedMutex mutex{"LoggingUi"};
        std::vector<Message> messages;
    };
}
