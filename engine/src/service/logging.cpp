#include "engine/service/logging.h"

#include "engine/config/config.h"
#include "engine/config/builder.h"

#include "engine/threads/service.h"

#include <iostream>

using namespace simcoe;

namespace chrono = std::chrono;

using systime_point = chrono::system_clock::time_point;

// logging

#define COLOUR_RED "\x1B[1;31m"    ///< ANSI escape string for red
#define COLOUR_GREEN "\x1B[1;32m"  ///< ANSI escape string for green
#define COLOUR_YELLOW "\x1B[1;33m" ///< ANSI escape string for yellow
#define COLOUR_BLUE "\x1B[1;34m"   ///< ANSI escape string for blue
#define COLOUR_PURPLE "\x1B[1;35m" ///< ANSI escape string for purple
#define COLOUR_CYAN "\x1B[1;36m"   ///< ANSI escape string for cyan
#define COLOUR_RESET "\x1B[0m"     ///< ANSI escape reset

namespace {
    constexpr auto kLevelNames = std::to_array<std::string_view>({
        /*eAssert*/ "PANIC",
        /*eError*/  "ERROR",
        /*eWarn*/   "WARN",
        /*eInfo*/   "INFO",
        /*eDebug*/  "DEBUG",
    });

    constexpr auto kLevelColours = std::to_array<std::string_view>({
        /*eAssert*/ COLOUR_CYAN,
        /*eError*/  COLOUR_RED,
        /*eWarn*/   COLOUR_YELLOW,
        /*eInfo*/   COLOUR_GREEN,
        /*eDebug*/  COLOUR_PURPLE,
    });

    // ⣩⠳⢿⣦⣄⡀⠀⠀⠀⠀⠀⠸⣯⣳⢿⣦⡄⡀⣘⣿⣚⢷⠶⠞⠛⠚⠓⠛⠖⠲⠽⢦⡶⣟⣱⣿⣿⡶⠛⢠⡴⠋⠀⠀⠀⠀⠀⠀⠀⢀
    // ⠛⢿⣷⣮⣝⡿⢿⣶⣦⣄⡀⠀⠈⢻⣷⣽⣿⢿⠛⡉⢠⡀⣆⢐⡢⢘⡠⢍⠒⣄⠲⣄⠐⠈⣡⡿⠋⣰⣾⡟⠀⠀⠀⠀⠀⢀⣀⡤⢖⣫
    // ⠀⠀⠀⠉⠙⠛⠷⣮⣽⣻⢿⣶⣤⣾⣿⡿⠑⣂⢂⠶⡀⠇⠶⣲⢃⡙⣖⢣⠹⣬⡳⣈⠞⡸⠟⠀⢠⠉⠹⢮⡄⣀⣤⠶⣞⡫⢽⠒⠋⠁
    // ⣀⣀⡀⠀⠀⠀⠀⠀⠙⠻⢿⣶⣯⡿⠋⢠⠘⡴⢋⡰⠙⡼⣉⢯⣥⢺⠬⣳⡹⣎⢵⢋⠞⢁⡀⢒⡀⣍⠰⠈⢹⣭⡞⠗⣋⠴⣳⢞⣧⣀
    // ⠉⠉⠛⠛⠶⠻⠿⢿⢶⣦⣾⡿⠋⠀⣬⢃⡟⢠⢆⡙⢇⠶⣩⠌⣻⣎⢳⣎⢗⣚⠎⣁⠴⣊⠵⠊⡔⢬⠂⠩⠄⢺⣅⣠⢏⡓⠄⠊⠒⠭
    // ⠋⠙⠙⠚⠒⠶⣦⣦⣤⣭⠟⠀⠀⡁⠂⠣⠌⠣⢎⡙⠈⠆⢄⠋⠴⣹⢾⡹⢊⣡⠼⣎⡿⢧⡟⣹⠞⠢⠍⠒⠉⠀⣽⡤⢦⡰⢄⡠⣌⢳
    // ⣀⣀⣤⣤⣶⡴⣶⣿⣯⠁⠀⠀⢧⠐⣈⡕⢢⠳⣤⢢⢁⡀⠀⡰⢢⣍⠒⣥⠿⣌⡭⣤⢩⣦⣰⢆⢯⣐⢳⠢⢒⡀⢹⣿⡷⢛⣮⣵⣚⣯
    // ⠻⠿⠶⠷⠾⠷⢿⣿⣿⠀⠀⠆⢈⣐⠧⣬⣣⣷⢮⣳⣍⠲⡅⠆⡽⣞⡿⣤⣍⡙⠻⣜⢫⠷⣍⡿⢌⡓⣊⡵⠈⠀⣼⡿⣇⣀⡁⠉⠁⠀
    // ⠀⠀⠀⠀⠀⠀⣰⣿⡇⢠⣃⠎⣠⠛⡗⣺⣅⢫⢷⡱⢊⡙⢴⣛⢷⣻⠾⣝⣭⡓⢶⣄⣃⠨⣔⠡⢎⢕⣰⠃⡜⠀⣿⣶⣮⣭⣭⣙⣓⣒
    // ⠀⠀⣀⣠⣤⣶⣿⣿⡁⣧⢆⡎⡔⢯⣵⡖⡹⢨⡳⢎⡳⣽⢶⣯⣿⣳⣟⡾⡜⣽⣫⠖⣽⡲⣌⠙⡌⣾⠏⡴⠁⢰⣿⣿⣷⣈⠉⠉⠉⠁
    // ⣿⡟⣟⣯⣽⣷⣿⣏⠱⣍⠞⣴⡘⠮⣰⡴⢻⠽⣙⢯⡳⣮⣿⣳⣩⢗⡼⣶⢏⠲⣭⢳⢌⡳⣬⣛⢰⣿⠆⠀⠌⣾⣷⣭⣛⠻⢿⣶⣤⠀
    // ⣿⣿⣿⣿⣿⣿⣿⣿⣦⡜⢛⣭⣶⢿⣝⣦⣑⡚⣭⡛⢷⡩⢶⢻⡇⢯⠛⡬⢍⢦⢱⢫⢿⡱⢢⣽⠟⢡⣄⡩⢶⠟⣽⣿⠿⣿⣷⣮⣝⡣
    // ⣿⢯⣳⢛⡾⣽⡿⢛⣯⣵⣿⣿⣯⡞⡱⢞⡽⣻⣶⡽⣶⣻⣯⣷⢮⣬⣭⣜⠉⣶⡈⣧⠘⡱⡵⢎⣲⣑⡾⣝⣷⣌⢟⣿⣧⡀⠀⠉⠛⠛
    // ⣟⠦⡁⠋⠀⣡⣶⣿⣿⢯⣿⢰⣿⣿⣷⡾⣄⣷⣽⣟⡿⣿⣿⣿⣳⣟⣾⣻⣧⡘⣯⢹⣆⡔⣛⣮⢷⣿⢱⣯⣿⢿⣷⣎⡿⣿⣷⣄⠀⠀
    // ⠏⣁⣄⢠⣄⢣⣞⣽⣿⣹⠇⣾⣿⣿⣿⣿⣝⣾⣻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣹⢿⡧⣿⣞⡱⣞⣿⣿⣾⡿⣿⣿⠋⠻⢿⣦⣝⢻⣷⣄
    // ⣾⣷⣞⡷⣮⢷⣯⣿⣿⡯⢼⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣯⣟⣿⡼⣿⣳⡽⣿⣳⣿⣾⡿⠃⠀⠀⠀⠈⠻⣷⣎⡻
    // ⠟⠻⣿⣿⣿⣟⣿⠟⣹⡿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣻⢷⡯⢿⣧⢹⣷⡛⢿⢿⣿⡟⠁⠀⠀⠀⠀⠀⠀⠈⠻⢷
    // ⠀⠀⠀⠉⠉⠉⠀⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣻⣽⡾⣯⣟⣾⣻⢿⣎⢿⣹⡌⢸⣏⠀⡄⠀⠀⠀⠀⠀⠀⠀⠀⠘
    // ⠀⠀⠀⠀⠀⠀⠀⠀⠙⣿⣟⡭⣿⣯⣽⣿⣿⣿⣿⣿⣿⣿⣿⣷⣻⣯⢷⣟⣷⣻⢾⣽⣿⢿⡤⣿⡜⣽⠿⣆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
    // ⠀⠀⠀⠀⠀⠀⠀⠀⢠⣿⣿⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣽⣷⣿⣻⣾⢯⣟⣿⡾⣣⣾⣇⢹⣿⡿⣖⢿⣧⡀⠀⠀⠀⠀⠀⠀⠀⠀
    // ⠀⠀⠀⠀⠀⠀⠀⣰⣿⣿⣿⡟⢾⣿⣿⣟⣿⣿⣿⣿⣿⡿⣟⣯⣷⢿⣻⣾⣿⡾⣻⣼⣿⣿⣿⡄⢿⣿⣿⣳⣭⡻⣧⣄⠀⠀⠀⠀⠀⠀
    // ⠀⠀⠀⠀⠀⠀⢀⣿⣿⣿⣿⣿⢳⣶⣭⣿⣿⣿⣿⣽⣾⡿⣟⣿⣾⣿⡿⢛⣥⣾⣿⣿⣿⣷⣻⣧⠸⣿⣿⣿⣷⡿⣝⣻⣷⣤⡀⠀⠀⠀
    // ⠀⠀⠀⠀⠀⠀⠠⣿⣿⣿⣿⣿⣧⡻⣿⣿⣿⣟⣿⣯⣿⡿⢿⢛⣫⣴⣾⣿⣿⣿⣿⣿⣿⣾⣟⣿⡆⢹⣿⢿⣾⣿⣿⣷⣾⠽⣿⣧⡄⠀
    // ⠀⠀⠀⠀⠀⠀⠐⣿⣿⣿⣿⣿⣿⣿⣶⣯⣟⣻⣿⣯⣽⣿⡙⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣿⣻⣷⠀⢻⣯⢾⡽⣿⣾⣿⣿⣳⣟⣿⣆
    // ⠀⠀⠀⠀⠀⠀⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣾⣿⣧⢻⣾⣿⣿⣿⣿⣿⣿⣿⣿⣟⡾⣽⡿⠀⠀⣿⣯⢿⣿⣿⣿⣿⣿⣿⣾⣽
    template<bool Colour>
    std::string formatMessageInner(systime_point time, LogLevel level, auto name, std::string_view msg) {
        constexpr const char *kFormat = std::is_same_v<decltype(name), std::string_view>
            ? "[{:%X}.{:<3}:{}{:^5}{}:{:^8}] {}"
            : "[{:%X}.{:<3}:{}{:^5}{}:{:^#8x}] {}";

        auto ms = chrono::duration_cast<chrono::milliseconds>(time.time_since_epoch()).count() % 1000;
        return std::format(kFormat,
            time, ms,
            Colour ? kLevelColours.at(level) : "", kLevelNames.at(level), Colour ? COLOUR_RESET : "",
            name, msg
        );
    }
}

// sinks

std::string logging::formatMessage(const LogMessage& msg) {
    auto name = ThreadService::getThreadName(msg.threadId);
    if (name.empty()) {
        return formatMessageInner<false>(msg.time, msg.level, msg.threadId, msg.msg);
    }

    auto shortName = name.substr(0, std::min(name.size(), size_t(8)));

    return formatMessageInner<false>(msg.time, msg.level, shortName, msg.msg);
}

std::string logging::formatMessageColour(const LogMessage& msg) {
    auto name = ThreadService::getThreadName(msg.threadId);
    if (name.empty()) {
        return formatMessageInner<true>(msg.time, msg.level, msg.threadId, msg.msg);
    }

    auto shortName = name.substr(0, std::min(name.size(), size_t(8)));
    return formatMessageInner<true>(msg.time, msg.level, shortName, msg.msg);
}

void ISink::addLogMessage(LogLevel level, threads::ThreadId threadId, MessageTime time, std::string_view msg) {
    if (!bSplitLines) {
        LogMessage data = { level, threadId, time, msg };
        accept(data);
    } else {
        // split on each newline and send as a new message
        size_t pos = 0;
        while (pos < msg.size()) {
            auto next = msg.find('\n', pos);
            if (next == std::string_view::npos) {
                next = msg.size();
            }

            LogMessage data = { level, threadId, time, msg.substr(pos, next - pos) };
            accept(data);
            pos = next + 1;
        }
    }
}

void StreamSink::accept(const LogMessage& msg) {
    std::lock_guard guard(mutex);
    os << logging::formatMessageColour(msg) << std::endl;
}

LoggingService::LoggingService() {
    addLogSink(new StreamSink(std::cout));
}

namespace {
    // config
    using CfgMap = config::Table;
    using LevelConfig = config::Enum<LogLevel>;
    const LevelConfig::NameMap kLevelMap = {
        { "panic", LogLevel::eAssert },
        { "error", LogLevel::eError },
        { "warn", LogLevel::eWarn },
        { "info", LogLevel::eInfo },
        { "debug", LogLevel::eDebug },
    };
}

const config::ISchemaBase *LoggingService::gConfigSchema
    = CFG_DECLARE(LoggingService, "logging", {
        CFG_FIELD_ENUM("level", level, kLevelMap)
    });

bool LoggingService::createService() {
    using Hmm = decltype(getFieldType(&LoggingService::level));
    static_assert(std::is_same_v<Hmm, LogLevel>);

    LOG_INFO("log level: {}", kLevelNames.at(level));

    return true;
}

void LoggingService::destroyService() {

}

// private interface

void LoggingService::sendMessage(LogLevel msgLevel, std::string_view msg) {
    auto threadId = ThreadService::getCurrentThreadId();
    auto currentTime = chrono::system_clock::now();

    mt::read_lock guard(lock);
    for (ISink *pSink : sinks) {
        pSink->addLogMessage(msgLevel, threadId, currentTime, msg);
    }
}

void LoggingService::throwAssert(std::string_view msg) {
    sendMessage(eAssert, msg);
    throw std::runtime_error(std::string(msg));
}

void LoggingService::addLogSink(ISink *pSink) {
    mt::write_lock guard(lock);
    sinks.push_back(pSink);
}
