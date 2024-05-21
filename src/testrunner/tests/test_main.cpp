//
// Created by Fredrik Kling on 18.08.22.
//
#include "ext_testinterface/testinterface.h"
#include "logger.h"
#include "config.h"

extern "C" {
    DLL_EXPORT int test_main(ITesting *t);
    DLL_EXPORT int test_exit(ITesting *t);

    DLL_EXPORT int test_ifv2(ITesting *t);
    DLL_EXPORT int test_ifv2_assert(ITesting *t);
    DLL_EXPORT int test_ifv2_exit(ITesting *t);

    DLL_EXPORT int test_prepost(ITesting *t);
    DLL_EXPORT int test_prepost_dummy(ITesting *t);
    DLL_EXPORT int test_prepost_exit(ITesting *t);

    DLL_EXPORT int test_prefail(ITesting *t);
    DLL_EXPORT int test_prefail_dummy(ITesting *t);
    DLL_EXPORT int test_prefail_exit(ITesting *t);

    DLL_EXPORT int test_postfail(ITesting *t);
    DLL_EXPORT int test_postfail_dummy(ITesting *t);
    DLL_EXPORT int test_postfail_exit(ITesting *t);

    DLL_EXPORT int test_abortmod(ITesting *t);
    DLL_EXPORT int test_abortmod_dummy(ITesting *t);
    DLL_EXPORT int test_abortmod_exit(ITesting *t);

    DLL_EXPORT int test_abortall(ITesting *t);
    DLL_EXPORT int test_abortall_dummy(ITesting *t);
    DLL_EXPORT int test_abortall_exit(ITesting *t);

}

using namespace trun;

// this is the global test main - if you need to setup something for the whole test-suite, do it here..
DLL_EXPORT int test_main(ITesting *t) {
    // Don't do anything - just call it - as it performs initialization on the logger and other things
    // otherwise it will be reinitialized later by functions under test
    Config::Instance();
    // Now set the log-level, we just want errors (this is for the library and not for the testrunner)
    gnilk::Logger::SetAllSinkDebugLevel(gnilk::LogLevel::kError);

    // Test the module dependency
    t->ModuleDepends("mdepmodA", "mdepmodB");
    t->ModuleDepends("mdepmodB", "mdepmodC,mdepmodD");

    return kTR_Pass;

    // FIXME: This doesn't work... (I've changed the config stuff)

    // Testing the v2 query-interface features
    ITestingConfig *tr_config = nullptr;
    t->QueryInterface(ITestingConfig_IFace_ID, (void **)&tr_config);
    TR_ASSERT(t, tr_config != nullptr);

    TRUN_ConfigItem configItems[10];

    size_t nMaxItems = tr_config->List(0, nullptr);
    TR_ASSERT(t, nMaxItems != 0);

    size_t nItems = tr_config->List(10, &configItems[0]);
    TR_ASSERT(t, nItems == 5);

    for(int i=0;i<(int)nItems;i++) {
        printf("%d:%s = ", i, configItems[i].name);
        switch(configItems[i].value_type) {
            case kTRCfgType_Str :
                printf("%s", configItems[i].value.str);
                break;
            case kTRCfgType_Bool :
                printf("%s", (configItems[i].value.boolean)?"true":"false");
                break;
            case kTRCfgType_Num :
                printf("%d", configItems[i].value.num);
                break;
        }
        printf("\n");
    }

    size_t nItems2 = tr_config->List(2, &configItems[0]);
    TR_ASSERT(t, nItems2 == 2);

    return kTR_Pass;
}
DLL_EXPORT int test_exit(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_ifv2(ITesting *t) {
    printf("ifv2 - module main\n");


    return kTR_Pass;
}

DLL_EXPORT int test_ifv2_exit(ITesting *t) {
        printf("ifv2 - module exit\n");
        return kTR_Pass;
}

DLL_EXPORT int test_ifv2_assert(ITesting *t) {
    printf("ifv2 - module exit\n");
    TR_ASSERT(t, false);
    return kTR_Pass;
}

//
// Test failing during pre-hook
//
static int prefail_pre(ITesting *t) {
    TR_ASSERT(t, false);
    return kTR_Pass;
}
static int prefail_post(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_prefail(ITesting *t) {
    t->SetPreCaseCallback(prefail_pre);
    t->SetPostCaseCallback(prefail_post);
    return kTR_Pass;
}
DLL_EXPORT int test_prefail_dummy(ITesting *t) {
    printf(":::: SHOULD NOT BE SEEN:::\n");
    return kTR_Pass;
}
DLL_EXPORT int test_prefail_exit(ITesting *t) {
    return kTR_Pass;
}

//
// Test failing during post-hook
//

static int postfail_pre(ITesting *t) {
    return kTR_Pass;
}
static int postfail_post(ITesting *t) {
    TR_ASSERT(t, false);
    return kTR_Pass;
}

DLL_EXPORT int test_postfail(ITesting *t) {
    t->SetPreCaseCallback(postfail_pre);
    t->SetPostCaseCallback(postfail_post);
    return kTR_Pass;
}
DLL_EXPORT int test_postfail_dummy(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_postfail_exit(ITesting *t) {
    return kTR_Pass;
}

//
static bool was_pre_called = false;
static bool was_post_called = false;


static int prepost_precase_cb(ITesting *t) {
    was_pre_called = true;
    return kTR_Pass;
}
static int prepost_postcase_cb(ITesting *t) {
    was_post_called = true;
    return kTR_Pass;
}
DLL_EXPORT int test_prepost(ITesting *t) {
    was_pre_called = false;
    was_post_called = false;

    t->SetPreCaseCallback(prepost_precase_cb);
    t->SetPostCaseCallback(prepost_postcase_cb);
    return kTR_Pass;
}
DLL_EXPORT int test_prepost_dummy(ITesting *t) {
        return kTR_Pass;
}
DLL_EXPORT int test_prepost_exit(ITesting *t) {
    TR_ASSERT(t, was_pre_called);
    TR_ASSERT(t, was_post_called);
    return kTR_Pass;
}

DLL_EXPORT int test_abortmod(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_abortmod_dummy(ITesting *t) {
    return kTR_FailModule;
}
DLL_EXPORT int test_abortmod_exit(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_abortall(ITesting *t) {
        return kTR_Pass;
}
DLL_EXPORT int test_abortall_dummy(ITesting *t) {
        return kTR_FailAll;
}
DLL_EXPORT int test_abortall_exit(ITesting *t) {
        return kTR_Pass;
}
