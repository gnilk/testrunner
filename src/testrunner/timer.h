#pragma once

#include <stdint.h>
#include <mach/mach_time.h>

class Timer {
public:
    Timer();
    void Reset();
    double Sample();
private:
    mach_timebase_info_data_t    timebaseInfo;
    uint64_t tStart;
};