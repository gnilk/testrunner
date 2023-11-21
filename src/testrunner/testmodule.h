#pragma once
#include "platform.h"

#include "dynlib.h"
#include "config.h"
#include "logger.h"
#include "testresult.h"
#include "responseproxy.h"
#include "testinterface.h"
#include "testhooks.h"
#include "testfunc.h"
#include "strutil.h"
#include <string>

namespace trun {
//
// TestModule is a collection of testable functions
//  test_<library>_<case>
//
    class TestModule {
    public:
        TestModule(std::string _name) :
                name(_name),
                bExecuted(false),
                mainFunc(nullptr),
                exitFunc(nullptr),
                cbPreHook(nullptr),
                cbPostHook(nullptr) {};

        __inline bool Executed() const { return bExecuted; }

        bool ShouldExecute() const;

        TestFunc *TestCaseFromName(const std::string &caseName) const;
        void SetDependencyForCase(const char *caseName, const char *dependencyList);
        void ResolveDependencies();
    public:
        std::string name;
        bool bExecuted;
        TestFunc *mainFunc;
        TestFunc *exitFunc;

        TRUN_PRE_POST_HOOK_DELEGATE *cbPreHook;
        TRUN_PRE_POST_HOOK_DELEGATE *cbPostHook;

        std::vector<TestFunc *> testFuncs;
    };
}