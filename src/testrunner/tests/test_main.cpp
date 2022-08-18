//
// Created by Fredrik Kling on 18.08.22.
//
#include "../testinterface.h"
#include "../logger.h"
#include "../config.h"
extern "C" {
    DLL_EXPORT int test_main(ITesting *t);
}

// this is the global test main - if you need to setup something for the whole test-suite, do it here..
DLL_EXPORT int test_main(ITesting *t) {

    // Don't do anything - just call it - as it performs initialization on the logger and other things
    // otherwise it will be reinitialized later by functions under test
    Config::Instance();
    // Now set the log-level, we just want errors (this is for the library and not for the testrunner)
    gnilk::Logger::SetAllSinkDebugLevel(gnilk::Logger::kMCError);

    return kTR_Pass;
}
