#pragma once

#include <stdint.h>
#if defined(WIN32)
#include <winnt.h>
#elif defined(__linux)
#include <chrono>
#else
#include <mach/mach_time.h>
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
#elif defined(__linux)
    std::chrono::steady_clock::time_point time_start;
#else
    mach_timebase_info_data_t    timebaseInfo;
#endif
    [[maybe_unused]] uint64_t tStart;
};