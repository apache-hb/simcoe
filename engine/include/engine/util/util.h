#pragma once

namespace simcoe::util {
    struct Once {
        template<typename F>
        void operator()(F&& func) {
            if (!bReported) {
                bReported = true;
                func();
            }
        }

        void reset();
    private:
        bool bReported = false;
    };
}
