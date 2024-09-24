//
// Created by gnilk on 17.09.2024.
//
#include "../testinterface_internal.h"
#include "../testfunc.h"
#include "../dynlib.h"
#include "../strutil.h"
#include "../testrunner.h"
#include <vector>
#include <string>

using namespace trun;


extern "C" {
DLL_EXPORT int test_assert(ITesting *t);
DLL_EXPORT int test_assert_single(ITesting *t);
DLL_EXPORT int test_assert_multiple(ITesting *t);
}
DLL_EXPORT int test_assert(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_assert_single(ITesting *t) {
    TR_ASSERT(t, true==false);
    return kTR_Pass;
}
DLL_EXPORT int test_assert_multiple(ITesting *t) {
    // t->SetDescription("Check if multiple assert values are allowed");

    ITestingConfig *trConfig = {};
    t->QueryInterface(ITestingConfig_IFace_ID, (void **)&trConfig);
    TR_ASSERT(t, trConfig != nullptr);
    TRUN_ConfigItem continueOnAssert = {};
    trConfig->Get("continue_on_assert", &continueOnAssert);
    // Verify - this is not an error..
    TR_REQUIRE(t, (continueOnAssert.isValid) && (continueOnAssert.value_type == kTRCfgType_Bool) && (continueOnAssert.value.boolean == true), "Must run with 'continue on assert' enabled!");

    TR_ASSERT(t, 1==2);
    TR_ASSERT(t, true==false);
    return kTR_Pass;
}
