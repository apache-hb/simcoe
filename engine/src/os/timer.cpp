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

Timer::Timer() 
    : frequency(getFrequency())
    , start(getCounter())
{ }

float Timer::now() {
    size_t counter = getCounter();
    return float(counter - start) / frequency;
}
