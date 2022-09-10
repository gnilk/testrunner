#pragma once

#include <stdint.h>
#if defined(WIN32)
#include <winnt.h>
#elif defined(__linux) || defined(APPLE)
#include <chrono>
#endif

class Timer {
public:
    Timer();
    void Reset();
    // returns time since reset as seconds...
    double Sample();
private:
#ifdef WIN32
    LARGE_INTEGER StartingTime, EndingTime, ElapsedMicroseconds;
    LARGE_INTEGER Frequency;
#elif defined(__linux) || defined(APPLE)
    std::chrono::steady_clock::time_point time_start;
#endif
    [[maybe_unused]] uint64_t tStart;
};