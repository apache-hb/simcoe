#pragma once

#include "engine/log/log.h"

#include <fstream>

namespace simcoe::log {
    // sinks are externally synchronized
    struct ConsoleSink final : ISink {
        ConsoleSink();

        void accept(const Message& msg) override;

    private:
        bool bColourSupport;
    };

    struct FileSink final : ISink {
        FileSink();

        void accept(const Message& msg) override;

    private:
        void openFile(std::string_view path);

        std::ofstream os;
    };
}
