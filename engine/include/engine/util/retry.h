#pragma once

namespace simcoe::util {
    struct Retry {
        Retry(float retryInterval = 3.f);

        void reset();
        bool shouldRetry(float time);
    private:
        float retryInterval; ///< how many seconds to wait before retrying

        float timeSinceLastTry(float time) const;
        float lastTime = 0.f; ///< last time we tried
    };
}
