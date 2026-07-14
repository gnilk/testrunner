//
// trunmcu.h - public facade for the zero-alloc MCU test engine (engine #3).
//
// Source-compatible with the trunembedded facade (Initialize / AddTestCase / RunTests /
// SetVerbose) for drop-in use; adds SetOutputSink and a RunResult return value.
//
// The whole engine is no-heap, no-thread, no-exception. Mid-body abort (a failing V1
// assert, or Fatal/Abort) uses setjmp/longjmp - see mcu_testing.{h,cpp}.
//
#ifndef TRUN_MCU_TRUNMCU_H
#define TRUN_MCU_TRUNMCU_H

#include <stddef.h>
#include "testinterface.h"      // frozen contract: V2 (default) or V1 (TRUN_USE_V1)

namespace trun {

    extern "C" {
        typedef int (*PTESTCASE)(ITesting *param);
    }

    // Register a test case from its symbol literal. Natively trun resolves these as
    // exported symbols; embedded you register them explicitly. The name MUST be a
    // string with static lifetime (a literal) - the engine stores the pointer, never a copy.
    #define TRUN_ADD_TEST(_TC_) do { trun::AddTestCase(#_TC_, _TC_); } while(0)

    // The engine drains all reporting through an output sink (default: stdout). The sink
    // returns a result so the transport can tell the engine whether to keep going.
    enum class OutputSinkResult {
        kOk,            // wrote the whole chunk; continue
        kRetry,         // wrote nothing (all-or-nothing); engine re-issues the same chunk
        kErrorContinue, // write failed; drop this output but keep running tests
        kErrorAbort,    // fatal transport error; stop the run
    };
    typedef OutputSinkResult (*OutputSinkFunc)(const char *data, size_t length);

    // Outcome of a full RunTests pass.
    enum class RunResult {
        kOk,        // every executed case passed
        kFailed,    // one or more cases failed
        kAborted,   // run cut short (Abort / kTR_FailAll, or sink returned kErrorAbort)
    };

    void Initialize();
    void AddTestCase(const char *symbolName, PTESTCASE func);
    RunResult RunTests(const char *moduleFilter, const char *caseFilter);

    // Verbosity: 0 - errors only, 1 - informational, 2 - debug.
    void SetVerbose(int lvl);

    // Override the output sink; pass nullptr to restore the default stdout sink.
    void SetOutputSink(OutputSinkFunc fn);
}

#endif // TRUN_MCU_TRUNMCU_H
