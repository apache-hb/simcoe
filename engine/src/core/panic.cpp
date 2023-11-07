#include "engine/core/panic.h"

#include "engine/core/error.h"
#include "engine/log/service.h"
#include "engine/service/platform.h"

#include <iostream>

using namespace simcoe;

// fall back between services depending on whats available

void core::panic(const PanicInfo &info, std::string_view msg) {
    if (PlatformService::getState() & ~eServiceFaulted) {
        auto title = std::format("PANIC {}:{} @ {}", info.file, info.line, info.fn);
        PlatformService::message(title, msg);
        core::throwFatal(msg);
    }

    std::cout << "[PANIC " << info.file << ":" << info.line << " @ " << info.fn << "] " << msg << std::endl;
    core::throwFatal(msg);
}
