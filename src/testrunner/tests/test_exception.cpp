//
// Created by gnilk on 19.09.2024.
//
#include "../testinterface_internal.h"
#include <vector>
#include <string>


extern "C" {
DLL_EXPORT int test_exception(ITesting *t);
DLL_EXPORT int test_exception_throw(ITesting *t);
}
DLL_EXPORT int test_exception(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_exception_throw(ITesting *t) {
    throw std::exception();
    return kTR_Pass;
}
