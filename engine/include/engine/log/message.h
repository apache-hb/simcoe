#pragma once

#include "engine/log/sink.h"

#include <string>
#include <format>

namespace simcoe::log {
    struct PendingMessage {
        PendingMessage(std::string initial)
            : msg(std::move(initial))
        { }

        void addLine(std::string_view str);

        template<typename... A>
        void addLine(std::string_view fmt, A&&... args) {
            addLine(std::vformat(fmt, std::make_format_args(args...)));
        }

        void send(log::Level level);

    private:
        std::string msg;
    };
}
