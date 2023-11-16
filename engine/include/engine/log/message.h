#pragma once

#include "engine/log/log.h"

#include "vendor/fmtlib/fmt.h"

#include <string>

namespace simcoe::log {
    // TODO: bulk message support
    struct PendingMessage {
        PendingMessage(std::string initial)
            : msg(std::move(initial))
        { }

        void addLine(std::string_view str);

        template<typename... A>
        void addLine(std::string_view msg, A&&... args) {
            addLine(fmt::vformat(msg, fmt::make_format_args(args...)));
        }

        void send(log::Level level) const;

    private:
        std::string msg;
    };
}
