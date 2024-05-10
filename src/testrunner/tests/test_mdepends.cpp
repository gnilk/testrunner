//
// Created by gnilk on 02.05.24.
//
// Module dependency testing...
//

#include "../config.h"
#include "ext_testinterface/testinterface.h"

extern "C" {
DLL_EXPORT int test_mdepmodA(ITesting *t);
DLL_EXPORT int test_mdepmodB(ITesting *t);
DLL_EXPORT int test_mdepmodC(ITesting *t);
DLL_EXPORT int test_mdepmodD(ITesting *t);
}

//
// Note: Dependencies configured in 'test_main'
//

DLL_EXPORT int test_mdepmodA(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_mdepmodB(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_mdepmodC(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_mdepmodD(ITesting *t) {
    return kTR_Pass;
}
