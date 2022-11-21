/*-------------------------------------------------------------------------
 File    : timer.cpp
 Author  : FKling
 Version : -
 Orginal : 2020-01-08
 Descr   : Simple timer to measure duration between Reset/Sample call's

 Part of testrunner
 BSD3 License!
 
 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>

 </pre>
 
 
 \History
 - 2020.01.08, FKling, Implementation
 
 ---------------------------------------------------------------------------*/

#ifdef WIN32
#include <windows.h>
#include <winnt.h>
#endif
#include "timer.h"
#include <chrono>

using namespace trun;

typedef std::chrono::steady_clock Clock;

Timer::Timer() {
    Reset();
}

//
// Restet starting point for clock
//
void Timer::Reset() {
    time_start = Clock::now();
}

//
// Sample the time between last reset and now, return as double in seconds
// Note: This could be a one-liner but I had to debug and decided to leave it like this as it makes it easier to read..
//
double Timer::Sample() {
    double ret = 0.0;

    auto elapsed = Clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed - time_start).count();

    ret = ms / 1000.0f;
  

    return ret;
}

