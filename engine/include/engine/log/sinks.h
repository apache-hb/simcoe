#pragma once

#include "engine/log/log.h"

#include "engine/threads/mutex.h"

#include <fstream>

namespace simcoe::log {
    struct ConsoleSink final : ISink {
        ConsoleSink();

        void accept(const Message& msg) override;

    private:
        bool bColour;

        mt::Mutex mutex{"log::ConsoleSink"};
    };

    struct FileSink final : ISink {
        FileSink();

        void accept(const Message& msg) override;

    private:
        void openFile(std::string_view path);

        mt::Mutex mutex{"log::FileSink"};
        std::ofstream os;
    };
}
