#include "engine/log/sinks.h"

#include "engine/config/ext/builder.h"

#include <iostream>

using namespace simcoe;
using namespace simcoe::log;

ConsoleSink::ConsoleSink(bool bColour)
    : ISink(true)
    , bColour(bColour)
{
    CFG_DECLARE("console",
        CFG_FIELD_BOOL("colour", &bColour)
    );
}

void ConsoleSink::accept(const Message& msg) {
    std::lock_guard guard(mutex);
    auto it = bColour ? log::formatMessageColour(msg) : log::formatMessage(msg);
    std::cout << it << "\n";
}

FileSink::FileSink()
    : ISink(true)
{
    CFG_DECLARE("file",
        CFG_FIELD_STRING("path", &path) // TODO: setup callback
    );
}

void FileSink::accept(const Message& msg) {
    std::lock_guard guard(mutex);
    if (!os.is_open()) {
        return;
    }

    auto it = log::formatMessageColour(msg);
    os << it << "\n";
}
