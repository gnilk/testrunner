//
// Created by gnilk on 03.05.24.
//

//
// Created by gnilk on 03.05.24.
//
#include "testinterface.h"

#include <stdio.h>
#include <stdlib.h>

extern "C" {
DLL_EXPORT int test_ifv2(ITesting *t);
DLL_EXPORT int test_ifv2_dummy(ITesting *t);
DLL_EXPORT int test_ifv2_exit(ITesting *t);
}
static int preCallbackV2(ITesting *t) {
    printf("Pre V2\n");
    return kTR_Pass;
}
static int postCallbackV2(ITesting *t) {
    printf("Post V2\n");
    return kTR_Pass;
}
DLL_EXPORT int test_ifv2(ITesting *t) {
    t->SetPreCaseCallback(preCallbackV2);
    t->SetPostCaseCallback(postCallbackV2);
    return kTR_Pass;
}

DLL_EXPORT int test_ifv2_dummy(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_ifv2_exit(ITesting *t) {
    return kTR_Pass;
}
