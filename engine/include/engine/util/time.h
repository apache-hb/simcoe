#pragma once

#include "engine/os/system.h"

namespace simcoe::util {
    /**
     * @brief manages the time between ticks
     */
    struct TimeStepper {
        TimeStepper(float minimumDelta);

        /**
         * @brief tick the time stepper, blocks until the time since the last tick is more than minimumDelta
         *
         * @return float the time since the last tick
         */
        float tick();
    private:
        Clock clock;

        float minimumDelta;

        float lastTime;
    };
}
