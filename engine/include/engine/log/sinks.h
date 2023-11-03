#pragma once

#include "engine/log/log.h"

#include <fstream>

namespace simcoe::log {
    struct ConsoleSink final : ISink {
        ConsoleSink(bool bColour = true);

        void accept(const Message& msg) override;

    private:
        bool bColour;

        std::mutex mutex;
    };

    struct FileSink final : ISink {
        FileSink();

        void accept(const Message& msg) override;

    private:
        void openFile(std::string_view path);

        std::mutex mutex;
        std::string path;
        std::ofstream os;
    };
}
