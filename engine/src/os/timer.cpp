#include "engine/os/system.h"

using namespace simcoe;

namespace {
    size_t getFrequency() {
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        return frequency.QuadPart;
    }

    size_t getCounter() {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        return counter.QuadPart;
    }
}

Clock::Clock()
    : frequency(getFrequency())
    , start(getCounter())
{ }

float Clock::now() const {
    size_t counter = getCounter();
    return float(counter - start) / frequency;
}
