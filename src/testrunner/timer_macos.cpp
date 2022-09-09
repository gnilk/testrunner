/*-------------------------------------------------------------------------
 File    : timer.cpp
 Author  : FKling
 Version : -
 Orginal : 2018-10-18
 Descr   : Simple timer to measure duration between Reset/Sample call's


 Part of testrunner
 BSD3 License!
 
 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 
 \History
 - 2018.10.18, FKling, Implementation
 
 ---------------------------------------------------------------------------*/

#include "timer.h"
#include <mach/mach_time.h>
#include <CoreServices/CoreServices.h>

Timer::Timer() {
    Reset();
}

//
// Reset starting point for clock
//
void Timer::Reset() {
    tStart = mach_absolute_time();
    mach_timebase_info(&timebaseInfo);
}

//
// Sample the time between last reset and now, return as double in seconds
//
double Timer::Sample() {
    double ret;

	// Derived from: https://developer.apple.com/library/archive/qa/qa1398/_index.html
    uint64_t elapsed = mach_absolute_time() - tStart;
    uint64_t elapsedNano = elapsed * timebaseInfo.numer / timebaseInfo.denom;
	ret = (double)(elapsedNano) / (double)(1000000000.0);
    return ret;
}

