#include "engine/core/panic.h"

#include "engine/log/service.h"
#include "engine/service/platform.h"

#include <iostream>

using namespace simcoe;

// fall back between services depending on whats available

void core::panic(const PanicInfo &info, std::string_view msg) {
    std::vector<StackFrame> stacktrace;
    if (DebugService::getState() & eServiceCreated) {
        stacktrace = DebugService::backtrace();
    }

    for (const auto &frame : stacktrace) {
        LOG_ERROR("  {} @ {}", frame.symbol, frame.pc);
    }
    LOG_ERROR("[{}:{} @ {}] {}", info.file, info.line, info.fn, msg);

    if (PlatformService::getState() & ~eServiceFaulted) {
        auto title = std::format("PANIC {}:{} @ {}", info.file, info.line, info.fn);
        PlatformService::message(title, msg);
        throw std::runtime_error(std::string(msg));
    }

    std::cout << "[PANIC " << info.file << ":" << info.line << " @ " << info.fn << "] " << msg << std::endl;
    throw std::runtime_error(std::string(msg));
}
