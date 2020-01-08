#pragma once

#include <stdint.h>
#if defined(WIN32)
#include <winnt.h>
#elif defined(__linux__)
#else
#include <mach/mach_time.h>
#endif

class Timer {
public:
    Timer();
    void Reset();
    double Sample();
private:
#ifdef WIN32
    LARGE_INTEGER StartingTime, EndingTime, ElapsedMicroseconds;
    LARGE_INTEGER Frequency;
#elif defined(__linux__)
#else
    mach_timebase_info_data_t    timebaseInfo;
#endif
    uint64_t tStart;
};