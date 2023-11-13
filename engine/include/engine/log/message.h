#pragma once

#include "engine/log/log.h"

#include <string>
#include <format>

namespace simcoe::log {
    // TODO: bulk message support
    struct PendingMessage {
        PendingMessage(std::string initial)
            : msg(std::move(initial))
        { }

        void addLine(std::string_view str);

        template<typename... A>
        void addLine(std::string_view fmt, A&&... args) {
            addLine(std::vformat(fmt, std::make_format_args(args...)));
        }

        void send(log::Level level) const;

    private:
        std::string msg;
    };
}
