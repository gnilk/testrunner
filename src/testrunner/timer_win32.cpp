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
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>

 </pre>
 
 
 \History
 - 2018.10.18, FKling, Implementation
 
 ---------------------------------------------------------------------------*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winnt.h>

#include "timer.h"


Timer::Timer() {
    Reset();
}

//
// Restet starting point for clock
//
void Timer::Reset() {

    QueryPerformanceFrequency(&Frequency); 
    QueryPerformanceCounter(&StartingTime);
}

//
// Sample the time between last reset and now, return as double in seconds
//
double Timer::Sample() {
    double ret;


    QueryPerformanceCounter(&EndingTime);
    ret = EndingTime.QuadPart - StartingTime.QuadPart;
    ret /= Frequency.QuadPart;

    return ret;
}

