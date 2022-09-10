/*-------------------------------------------------------------------------
 File    : timer.cpp
 Author  : FKling
 Version : -
 Orginal : 2020-01-08
 Descr   : Linux timer to measure duration between Reset/Sample call's


 Note: I dislike std::chrono....    where did "simplicity" go????????

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

#include "timer.h"
#include <chrono>

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
//
double Timer::Sample() {
    double ret = 0.0;

    auto elapsed = Clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed - time_start).count();

    ret = ms / 1000.0f;
  

    return ret;
}

