#include "engine/core/error.h"

#include "engine/debug/service.h"
#include "engine/log/service.h"

using namespace simcoe;
using namespace simcoe::core;

static debug::Backtrace getBacktrace() {
    debug::Backtrace bt;
    if (DebugService::getState() & eServiceCreated) {
        bt = DebugService::backtrace();
    }

    return bt;
}

Error::Error(bool bFatal, std::string msg) noexcept
    : bFatal(bFatal)
    , message(std::move(msg))
    , stacktrace(getBacktrace())
{ 
    if (bFatal) __debugbreak();
}
