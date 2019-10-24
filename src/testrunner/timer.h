#pragma once

#include <stdint.h>
#ifdef WIN32
#include <winnt.h>
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
#else
    mach_timebase_info_data_t    timebaseInfo;
#endif
    uint64_t tStart;
};