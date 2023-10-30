#pragma once

#include <thread>
#include <functional>

#include "engine/core/macros.h"
#include "engine/core/win32.h"

namespace simcoe::threads {
    using ThreadId = std::thread::id;
    using ThreadStart = std::function<void()>;
}
