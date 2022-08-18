//
// Created by Fredrik Kling on 18.08.22.
//

#ifdef WIN32
#include <Windows.h>
#endif

#include <chrono>
#include <thread>
#include "../timer.h"
// using dev-version, instead of install version... self-hosting..
#include "../testinterface.h"

extern "C" {
    DLL_EXPORT int test_timer(ITesting *t);
    DLL_EXPORT int test_timer_sample(ITesting *t);
}

DLL_EXPORT int test_timer(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_timer_sample(ITesting *t) {
    Timer timer;
    auto tStart = timer.Sample();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    auto tEnd = timer.Sample();
    //printf("delta: %f - %f = %f\n", tEnd, tStart, tEnd - tStart);
    TR_ASSERT(t, (tEnd - tStart) >= 1.0);
    TR_ASSERT(t, (tEnd - tStart) < 1.1);
    return kTR_Pass;
}
