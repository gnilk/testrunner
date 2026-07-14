//
// mcu_testing.cpp - ITesting trampolines + per-case state for the MCU engine.
//
#include "mcu_testing.h"
#include <stdint.h>

using namespace trun;
using namespace trun::mcu;

namespace {

    // The pre/post hook signature differs by interface version (V2 returns int, V1 void).
#ifdef TRUN_USE_V1
    typedef void (*HookFn)(ITesting *);
#else
    typedef int (*HookFn)(ITesting *);
#endif

    struct RunState {
        Reporter *reporter = nullptr;
        bool continueOnAssert = false;
        HookFn preHook = nullptr;
        HookFn postHook = nullptr;
        jmp_buf *caseEnv = nullptr;
        CaseResult *caseResult = nullptr;
    };

    RunState g_state;

    void ReportLoc(const char *prefix, int line, const char *file, const char *fmt, va_list ap) {
        if (g_state.reporter != nullptr) {
            g_state.reporter->LocLine(prefix, line, file, fmt, ap);
        }
    }

    // Jump back to the runner's per-case setjmp. Only valid while a case is active.
    void AbortToRunner(int code) {
        if (g_state.caseEnv != nullptr) {
            longjmp(*g_state.caseEnv, code);
        }
    }

    void RecordAssert(int line, const char *file, const char *exp) {
        if (g_state.caseResult != nullptr) {
            g_state.caseResult->assertCount++;
            g_state.caseResult->failed = true;
        }
        if (g_state.reporter != nullptr) {
            g_state.reporter->Line("=== ASSERT:\t%s:%d '%s'\n",
                                   (file != nullptr) ? file : "?", line, (exp != nullptr) ? exp : "");
        }
    }

} // anonymous namespace

void trun::mcu::SetReporter(Reporter *reporter) {
    g_state.reporter = reporter;
}
void trun::mcu::SetContinueOnAssert(bool value) {
    g_state.continueOnAssert = value;
}
void trun::mcu::ResetHooks() {
    g_state.preHook = nullptr;
    g_state.postHook = nullptr;
}
bool trun::mcu::HasPreHook() {
    return g_state.preHook != nullptr;
}
bool trun::mcu::HasPostHook() {
    return g_state.postHook != nullptr;
}
int trun::mcu::InvokePreHook() {
    if (g_state.preHook == nullptr) {
        return kTR_Pass;
    }
#ifdef TRUN_USE_V1
    g_state.preHook(GetInterface());
    return kTR_Pass;
#else
    return g_state.preHook(GetInterface());
#endif
}
int trun::mcu::InvokePostHook() {
    if (g_state.postHook == nullptr) {
        return kTR_Pass;
    }
#ifdef TRUN_USE_V1
    g_state.postHook(GetInterface());
    return kTR_Pass;
#else
    return g_state.postHook(GetInterface());
#endif
}
void trun::mcu::BeginCase(jmp_buf *env, CaseResult *result) {
    g_state.caseEnv = env;
    g_state.caseResult = result;
}
void trun::mcu::EndCase() {
    g_state.caseEnv = nullptr;
    g_state.caseResult = nullptr;
}

//
// ITesting trampolines - C linkage, reach engine state through g_state.
//
extern "C" {

static void trp_Debug(int line, const char *file, const char *fmt, ...) {
    if ((g_state.reporter == nullptr) || (g_state.reporter->Verbose() < 2)) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    ReportLoc("DEBUG", line, file, fmt, ap);
    va_end(ap);
}

static void trp_Info(int line, const char *file, const char *fmt, ...) {
    if ((g_state.reporter == nullptr) || (g_state.reporter->Verbose() < 1)) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    ReportLoc("INFO", line, file, fmt, ap);
    va_end(ap);
}

static void trp_Warning(int line, const char *file, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    ReportLoc("WARN", line, file, fmt, ap);
    va_end(ap);
}

static void trp_Error(int line, const char *file, const char *fmt, ...) {
    if (g_state.caseResult != nullptr) {
        g_state.caseResult->errorCount++;
        g_state.caseResult->failed = true;
    }
    va_list ap;
    va_start(ap, fmt);
    ReportLoc("ERROR", line, file, fmt, ap);
    va_end(ap);
    // soft - test continues
}

static void trp_Fatal(int line, const char *file, const char *fmt, ...) {
    if (g_state.caseResult != nullptr) {
        g_state.caseResult->failed = true;
        g_state.caseResult->fatal = true;
    }
    va_list ap;
    va_start(ap, fmt);
    ReportLoc("FATAL", line, file, fmt, ap);
    va_end(ap);
    AbortToRunner(kJmpFatal);
}

static void trp_Abort(int line, const char *file, const char *fmt, ...) {
    if (g_state.caseResult != nullptr) {
        g_state.caseResult->failed = true;
        g_state.caseResult->aborted = true;
    }
    va_list ap;
    va_start(ap, fmt);
    ReportLoc("ABORT", line, file, fmt, ap);
    va_end(ap);
    AbortToRunner(kJmpAbort);
}

static void trp_CaseDepends(const char *caseName, const char *deps) {
    // Dependencies are not supported on the MCU engine (flat module -> case iteration).
    (void)caseName;
    (void)deps;
}

#ifdef TRUN_USE_V1

static void trp_AssertError(const char *exp, const char *file, const int line) {
    RecordAssert(line, file, exp);
    // V1 TR_ASSERT is a bare call with no return - the only way to stop mid-body is to jump.
    AbortToRunner(kJmpAssert);
}
static void trp_SetPreCaseCallback(void (*fn)(ITesting *)) {
    g_state.preHook = fn;
}
static void trp_SetPostCaseCallback(void (*fn)(ITesting *)) {
    g_state.postHook = fn;
}

#else

static kTRContinueMode trp_AssertError(const int line, const char *file, const char *exp) {
    RecordAssert(line, file, exp);
    // V2 cooperates: the TR_ASSERT macro returns kTR_Fail when we say 'leave'.
    return g_state.continueOnAssert ? kTRContinue : kTRLeave;
}
static void trp_SetPreCaseCallback(int (*fn)(ITesting *)) {
    g_state.preHook = fn;
}
static void trp_SetPostCaseCallback(int (*fn)(ITesting *)) {
    g_state.postHook = fn;
}
static void trp_ModuleDepends(const char *moduleName, const char *deps) {
    (void)moduleName;
    (void)deps;
}
static void trp_QueryInterface(uint32_t interfaceId, void **outPtr) {
    (void)interfaceId;
    if (outPtr != nullptr) {
        *outPtr = nullptr;
    }
}

#endif

} // extern "C"

// The vtable. Field order mirrors struct ITesting for the selected interface version.
static ITesting g_interface = {
    trp_Debug,
    trp_Info,
    trp_Warning,
    trp_Error,
    trp_Fatal,
    trp_Abort,
    trp_AssertError,
    trp_SetPreCaseCallback,
    trp_SetPostCaseCallback,
    trp_CaseDepends,
#ifndef TRUN_USE_V1
    trp_ModuleDepends,
    trp_QueryInterface,
#endif
};

ITesting *trun::mcu::GetInterface() {
    return &g_interface;
}
