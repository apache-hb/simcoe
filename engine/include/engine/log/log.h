#pragma once

#include "engine/config/config.h"

#include "engine/threads/thread.h"

namespace simcoe::log {
    using MessageTime = std::chrono::system_clock::time_point;

    enum Level {
        eAssert,
        eError,
        eWarn,
        eInfo,
        eDebug,

        eTotal
    };

    std::string_view toString(Level level);

    struct Message {
        Level level;

        threads::ThreadId threadId;
        MessageTime time;

        std::string_view msg;
    };

    std::string formatMessage(const Message& msg);
    std::string formatMessageColour(const Message& msg);

    struct ISink {
        virtual ~ISink() = default;

        ISink(bool bSplitLines = false)
            : bSplitLines(bSplitLines)
        { }

        virtual void accept(const Message& msg) = 0;

        void addLogMessage(Level level, threads::ThreadId threadId, MessageTime time, std::string_view msg);

    private:
        // should each line be sent as as seperate message?
        bool bSplitLines;
    };
}
