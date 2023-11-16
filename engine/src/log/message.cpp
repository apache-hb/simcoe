#include "engine/log/message.h"
#include "engine/log/service.h"

#include "engine/threads/service.h"

using namespace simcoe;
using namespace simcoe::log;

namespace chrono = std::chrono;

#define COLOUR_RED "\x1B[1;31m"    ///< ANSI escape string for red
#define COLOUR_GREEN "\x1B[1;32m"  ///< ANSI escape string for green
#define COLOUR_YELLOW "\x1B[1;33m" ///< ANSI escape string for yellow
#define COLOUR_BLUE "\x1B[1;34m"   ///< ANSI escape string for blue
#define COLOUR_PURPLE "\x1B[1;35m" ///< ANSI escape string for purple
#define COLOUR_CYAN "\x1B[1;36m"   ///< ANSI escape string for cyan
#define COLOUR_RESET "\x1B[0m"     ///< ANSI escape reset

namespace {
    constexpr auto kLevelColours = std::to_array<std::string_view>({
        /*eAssert*/ COLOUR_CYAN,
        /*eError*/  COLOUR_RED,
        /*eWarn*/   COLOUR_YELLOW,
        /*eInfo*/   COLOUR_GREEN,
        /*eDebug*/  COLOUR_PURPLE,
    });

    template<bool Colour>
    std::string formatMessageInner(MessageTime time, Level level, auto name, std::string_view msg) {
        constexpr const char *kFormat = std::is_same_v<decltype(name), std::string_view>
            ? "[{:>6%X}:{}{:5}{}:{:^8}] {}"
            : "[{:>6%X}:{}{:5}{}:{:^#8x}] {}";

        return fmt::format(kFormat,
            chrono::floor<chrono::milliseconds>(time),
            Colour ? kLevelColours.at(level) : "", toString(level), Colour ? COLOUR_RESET : "",
            name, msg
        );
    }
}

// fmt

std::string_view log::toString(log::Level level) {
    switch (level) {
    case log::eAssert: return "panic";
    case log::eError: return "error";
    case log::eWarn: return "warn";
    case log::eInfo: return "info";
    case log::eDebug: return "debug";
    default: return "unknown";
    }
}

// sinks

std::string log::formatMessage(const Message& msg) {
    auto name = threads::getThreadName(msg.threadId);
    if (name.empty()) {
        return formatMessageInner<false>(msg.time, msg.level, msg.threadId, msg.msg);
    }

    auto shortName = name.substr(0, std::min(name.size(), size_t(8)));

    return formatMessageInner<false>(msg.time, msg.level, shortName, msg.msg);
}

std::string log::formatMessageColour(const Message& msg) {
    auto name = threads::getThreadName(msg.threadId);
    if (name.empty()) {
        return formatMessageInner<true>(msg.time, msg.level, msg.threadId, msg.msg);
    }

    auto shortName = name.substr(0, std::min(name.size(), size_t(8)));
    return formatMessageInner<true>(msg.time, msg.level, shortName, msg.msg);
}

void ISink::addLogMessage(Level level, threads::ThreadId threadId, MessageTime time, std::string_view msg) {
    if (!bSplitLines) {
        Message data = { level, threadId, time, msg };
        accept(data);
    } else {
        // split on each newline and send as a new message
        size_t pos = 0;
        while (pos < msg.size()) {
            auto next = msg.find('\n', pos);
            if (next == std::string_view::npos) {
                next = msg.size();
            }

            Message data = { level, threadId, time, msg.substr(pos, next - pos) };
            accept(data);
            pos = next + 1;
        }
    }
}

// pending message

void PendingMessage::addLine(std::string_view line) {
    msg += '\n';
    msg += line;
}

void PendingMessage::send(log::Level level) const {
    LoggingService::sendMessage(level, msg);
}
