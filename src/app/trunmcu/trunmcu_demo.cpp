//
// trunmcu_demo - exercises the zero-alloc MCU engine on the host.
//
// Built twice from this one source: V2 (default) and V1 (TRUN_USE_V1), so we validate
// both mid-body abort paths - V2 cooperative-return vs V1 forced longjmp - observably
// produce the same result (a failing assert stops its case; Fatal stops the module).
//
#include <stdio.h>
#include "trunmcu.h"
#include "mcu_runner.h"   // for kStaticFootprintBytes (internal - demo only)

extern "C" {
    int test_main(ITesting *t);
    int test_exit(ITesting *t);

    int test_alpha(ITesting *t);            // module 'alpha' main
    int test_alpha_pass(ITesting *t);
    int test_alpha_assertfail(ITesting *t);
    int test_alpha_afterassert(ITesting *t);

    int test_beta(ITesting *t);             // module 'beta' main
    int test_beta_fatal(ITesting *t);
    int test_beta_afterfatal(ITesting *t);
    int test_beta_exit(ITesting *t);        // module 'beta' exit
}

int test_main(ITesting *t) {
    (void)t;
    return kTR_Pass;
}
int test_exit(ITesting *t) {
    (void)t;
    return kTR_Pass;
}

int test_alpha(ITesting *t) {
    (void)t;
    return kTR_Pass;
}
int test_alpha_pass(ITesting *t) {
    TR_ASSERT(t, 1 == 1);
    return kTR_Pass;
}
int test_alpha_assertfail(ITesting *t) {
    TR_ASSERT(t, 1 == 2);                   // fails -> case stops here (V2 return / V1 longjmp)
    printf("    [BUG] line after a failing assert should NOT run\n");
    return kTR_Pass;
}
int test_alpha_afterassert(ITesting *t) {
    (void)t;                                // proves a failed assert only stops ITS case
    return kTR_Pass;
}

int test_beta(ITesting *t) {
    (void)t;
    return kTR_Pass;
}
int test_beta_fatal(ITesting *t) {
    t->Fatal(__LINE__, __FILE__, "fatal boom %d", 42);   // longjmp -> stops the module
    printf("    [BUG] line after Fatal should NOT run\n");
    return kTR_Pass;
}
int test_beta_afterfatal(ITesting *t) {
    (void)t;
    printf("    [BUG] case after a module Fatal should NOT run\n");
    return kTR_Pass;
}
int test_beta_exit(ITesting *t) {
    (void)t;                                // module exit still runs (teardown)
    return kTR_Pass;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    trun::Initialize();
    trun::SetVerbose(2);

    TRUN_ADD_TEST(test_main);
    TRUN_ADD_TEST(test_alpha);
    TRUN_ADD_TEST(test_alpha_pass);
    TRUN_ADD_TEST(test_alpha_assertfail);
    TRUN_ADD_TEST(test_alpha_afterassert);
    TRUN_ADD_TEST(test_beta);
    TRUN_ADD_TEST(test_beta_fatal);
    TRUN_ADD_TEST(test_beta_afterfatal);
    TRUN_ADD_TEST(test_beta_exit);
    TRUN_ADD_TEST(test_exit);

    trun::RunResult res = trun::RunTests("-", "-");

    const char *resstr = "ok";
    if (res == trun::RunResult::kFailed) {
        resstr = "failed";
    } else if (res == trun::RunResult::kAborted) {
        resstr = "aborted";
    }

    printf("\nRunTests result : %s (expected: failed - 2 intentional failures)\n", resstr);
    printf("Static footprint: %zu bytes\n", trun::mcu::kStaticFootprintBytes);
    return 0;
}
