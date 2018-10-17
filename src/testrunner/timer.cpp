#include "timer.h"
#include <mach/mach_time.h>
#include <CoreServices/CoreServices.h>

Timer::Timer() {
}

void Timer::Reset() {
    tStart = mach_absolute_time();
    mach_timebase_info(&timebaseInfo);
}

double Timer::Sample() {
    double ret;

	// Derived from: https://developer.apple.com/library/archive/qa/qa1398/_index.html
    uint64_t elapsed = mach_absolute_time() - tStart;

    uint64_t elapsedNano = elapsed * timebaseInfo.numer / timebaseInfo.denom;	

	ret = (double)(elapsedNano) / (double)(1000000000.0);

    return ret;
}

