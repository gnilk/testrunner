//
// mcu_testing.h - the zero-alloc ITesting implementation for the MCU engine.
//
// The ITesting vtable is plain C function pointers, so the trampolines reach engine
// state through a single file-static (single-threaded - no TLS needed). Forced mid-body
// stop is setjmp/longjmp: a V1 assert (bare, no return), Fatal and Abort all longjmp
// back to the per-case setjmp in the runner. A V2 assert/Error returns cooperatively.
//
#ifndef TRUN_MCU_TESTING_H
#define TRUN_MCU_TESTING_H

#include <setjmp.h>
#include "trunmcu.h"
#include "mcu_report.h"

namespace trun {
    namespace mcu {

        // Per-case result, accumulated by the ITesting trampolines.
        struct CaseResult {
            int assertCount = 0;
            int errorCount = 0;
            bool failed = false;    // soft fail (Error / cooperative assert)
            bool fatal = false;     // Fatal -> stop the rest of the module
            bool aborted = false;   // Abort -> stop the whole run
        };

        // longjmp codes (the value passed to longjmp, read back from setjmp).
        enum {
            kJmpAssert = 1,    // V1 forced assert
            kJmpFatal  = 2,
            kJmpAbort  = 3,
        };

        // Runner-side wiring.
        void SetReporter(Reporter *reporter);
        void SetContinueOnAssert(bool value);

        // Pre/post-case hooks, set by tests (typically in module main), consumed per module.
        void ResetHooks();
        bool HasPreHook();
        bool HasPostHook();
        int InvokePreHook();    // returns kTR_* (kTR_Pass if no hook)
        int InvokePostHook();   // returns kTR_* (kTR_Pass if no hook)

        // Per-case scope: 'env' is the longjmp target, 'result' accumulates callbacks.
        void BeginCase(jmp_buf *env, CaseResult *result);
        void EndCase();

        // The ITesting vtable handed to each test case.
        ITesting *GetInterface();

    } // namespace mcu
} // namespace trun

#endif // TRUN_MCU_TESTING_H
