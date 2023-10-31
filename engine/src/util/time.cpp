#include "engine/util/time.h"

using namespace simcoe;
using namespace simcoe::util;

TimeStep::TimeStep(float minimumDelta)
    : minimumDelta(minimumDelta)
    , lastTime(clock.now())
{ }

float TimeStep::waitForNextTick() {
    // sleep until the time since the last tick is more than minimumDelta
    auto now = clock.now();
    auto delta = std::chrono::duration<float>(now - lastTime);

    if (delta.count() < minimumDelta) {
        std::this_thread::sleep_for(std::chrono::duration<float>(minimumDelta - delta.count()));
    }

    lastTime = clock.now();
    return delta.count();
}
