    //
// Created by Fredrik Kling on 18.08.22.
//
#include "../testinterface.h"
#include "logger.h"
#include "../config.h"
#include <functional>
#include <string.h>
extern "C" {
    DLL_EXPORT int test_main(ITesting *t);
    DLL_EXPORT int test_exit(ITesting *t);

    DLL_EXPORT int test_ifv2(ITestingV2 *t);
    DLL_EXPORT int test_ifv2_assert(ITestingV2 *t);
    DLL_EXPORT int test_ifv2_exit(ITestingV2 *t);
}

using namespace trun;

// this is the global test main - if you need to setup something for the whole test-suite, do it here..
DLL_EXPORT int test_main(ITesting *t) {

    // Don't do anything - just call it - as it performs initialization on the logger and other things
    // otherwise it will be reinitialized later by functions under test
    Config::Instance();
    // Now set the log-level, we just want errors (this is for the library and not for the testrunner)
    gnilk::Logger::SetAllSinkDebugLevel(gnilk::LogLevel::kError);

    TRUN_IConfig *tr_config = nullptr;
    t->QueryInterface(1234, (void **)&tr_config);
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
                printf("%s", (configItems[i].value.b)?"true":"false");
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




