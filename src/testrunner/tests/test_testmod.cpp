//
// Created by gnilk on 21.11.23.
//
//
// Created by gnilk on 21.11.23.
//
#include <vector>
#include <string>
#include "testmodule.h"

#include "../testinterface.h"
#include "../strutil.h"

extern "C" {
    DLL_EXPORT int test_testmod(ITesting *t);
    DLL_EXPORT int test_testmod_shouldexec(ITesting *t);
}
DLL_EXPORT int test_testmod(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_testmod_shouldexec(ITesting *t) {
    trun::TestModule tmod("thread");
    trun::TestModule tmod2("atomicmutex");

    auto oldModules = trun::Config::Instance()->modules;
    trun::Config::Instance()->modules.push_back("!thread");
    trun::Config::Instance()->modules.push_back("atomicmutex");

    TR_ASSERT(t, !tmod.ShouldExecute());
    TR_ASSERT(t, tmod2.ShouldExecute());


    trun::Config::Instance()->modules = oldModules;

    trun::Config::Instance()->modules.push_back("thread");
    trun::Config::Instance()->modules.push_back("atomicmutex");

    TR_ASSERT(t, tmod.ShouldExecute());
    TR_ASSERT(t, tmod2.ShouldExecute());

    trun::Config::Instance()->modules = oldModules;
    return kTR_Pass;
}
