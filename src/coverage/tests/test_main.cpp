//
// tcov_utests - global test main/exit for the coverage-engine unit tests.
//
// Built into libtcov_utests.{so,dylib} and run with the framework's own runner:
//   ./trun lib/libtcov_utests.dylib
//
// See todo/tcov_cleanup.md (Phase 0) for why tcov grows a unit-test target.
//
#include "ext_testinterface/testinterface.h"

extern "C" {
    DLL_EXPORT int test_main(ITesting *t);
    DLL_EXPORT int test_exit(ITesting *t);
}

// Global setup shared across the whole tcov_utests suite (nothing needed yet).
DLL_EXPORT int test_main(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_exit(ITesting *t) {
    return kTR_Pass;
}
