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
    DLL_EXPORT int test_testmod_exit(ITesting *t);
    DLL_EXPORT int test_testmod_shouldexec(ITesting *t);
    // Enable this to test how testrunner proceeds for different exit codes
    // note: this test is interactive (for now)
    // DLL_EXPORT int test_testmod_enterexit(ITesting *t);
}
DLL_EXPORT int test_testmod(ITesting *t) {
    t->CaseDepends("shouldexec", "enterexit");
    return kTR_Pass;
}
DLL_EXPORT int test_testmod_exit(ITesting *t) {
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

DLL_EXPORT int test_testmod_enterexit(ITesting *t) {

    return kTR_FailModule;
}
