#pragma once

#include "engine/os/system.h"

namespace simcoe::util {
    /**
     * @brief manages the time between ticks
     */
    struct TimeStep {
        TimeStep(float minimumDelta);

        /**
         * @brief tick the time stepper, blocks until the time since the last tick is more than minimumDelta
         *
         * @return float the time since the last tick
         */
        float tick();

        float getDelta() const { return minimumDelta; }
    private:
        Clock clock;

        float minimumDelta;

        float lastTime;
    };
}
