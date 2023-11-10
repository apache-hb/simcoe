#pragma once

#include "engine/threads/service.h"

namespace simcoe::threads {
#if 0
    struct Scheduler {
        Scheduler();
        ~Scheduler();

        Thread& newThread(ThreadType type, std::string_view name, ThreadStart&& start);

        template<typename Rep, typename Period, typename F>
        Thread& newJob(std::string_view name, std::chrono::duration<Rep, Period> interval, F&& tick) {
            return newThread(eWorker, name, [tick, interval](std::stop_token stop) {
                while (!stop.stop_requested()) {
                    tick();
                    std::this_thread::sleep_for(interval);
                }
            });
        }

    private:
        struct ThreadData {
            ThreadType type;

            SubcoreIndex subcoreIdx;
            CoreIndex coreIdx;

            std::string_view name;
        };

        void setupThreadData();

        CoreIndices coreRanking; // core indices sorted from fastest to slowest
        ChipletIndices chipletRanking; // cluster indices sorted from fastest to slowest
        PackageIndices packageRanking; // package indices sorted from fastest to slowest

        uint16_t getCoreLoad(CoreIndex coreIdx) const;

        SubcoreIndex getLeastLoadedSubcore(CoreIndex coreIdx) const;
        SubcoreIndex getBestSubcore(ThreadType type, std::string_view name) const;

        mt::Mutex lock{"scheduler"};

        // the number of system threads running on each logical thread
        std::unordered_map<SubcoreIndex, uint16_t> subcoreUsage = {};

        std::vector<threads::Thread> threads;
    };
#endif
}
