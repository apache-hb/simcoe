#include "engine/util/time.h"

using namespace simcoe;
using namespace simcoe::util;

TimeStep::TimeStep(float minimumDelta)
    : minimumDelta(minimumDelta)
    , lastTime(clock.now())
{ }

float TimeStep::tick() {
    // sleep until the time since the last tick is more than minimumDelta

    float beforeSleep = lastTime;
    float timeSinceLastTick = clock.now() - lastTime;

    std::this_thread::sleep_for(std::chrono::duration<float>(minimumDelta - timeSinceLastTick));

    lastTime = clock.now();

    return lastTime - beforeSleep;
}
