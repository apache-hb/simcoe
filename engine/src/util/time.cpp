#include "engine/util/time.h"

using namespace simcoe;
using namespace simcoe::util;

TimeStepper::TimeStepper(float minimumDelta)
    : minimumDelta(minimumDelta)
    , lastTime(clock.now())
{ }

float TimeStepper::tick() {
    float time = clock.now();
    float delta = time - lastTime;

    while (delta < minimumDelta) {
        time = clock.now();
        delta = time - lastTime;
    }

    lastTime = time;

    return delta;
}
