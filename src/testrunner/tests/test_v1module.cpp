//
// Created by gnilk on 03.05.24.
//
#define TRUN_USE_V1
#include "../testinterface.h"

#include "logger.h"
#include "config.h"
#include <functional>
#include <string.h>

extern "C" {
DLL_EXPORT int test_ifv1(ITesting *t);
DLL_EXPORT int test_ifv1_dummy(ITesting *t);
DLL_EXPORT int test_ifv1_exit(ITesting *t);
}
static void preCallbackV1(ITesting *t) {
    printf("Pre V1\n");
    return;
}
static void postCallbackV1(ITesting *t) {
    printf("Post V1\n");
    return;
}
DLL_EXPORT int test_ifv1(ITesting *t) {
    t->SetPreCaseCallback(preCallbackV1);
    t->SetPostCaseCallback(postCallbackV1);
    return kTR_Pass;
}

DLL_EXPORT int test_ifv1_dummy(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_ifv1_exit(ITesting *t) {
    return kTR_Pass;
}
