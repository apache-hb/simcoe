#include "engine/util/retry.h"

using namespace simcoe;
using namespace simcoe::util;

Retry::Retry(float retryInterval)
    : retryInterval(retryInterval)
    , lastTime(-retryInterval)
{ }

void Retry::reset() {
    lastTime = -retryInterval;
}

bool Retry::shouldRetry(float time) {
    if (timeSinceLastTry(time) >= retryInterval) {
        lastTime = time;
        return true;
    }

    return false;
}

float Retry::timeSinceLastTry(float time) const {
    return time - lastTime;
}
