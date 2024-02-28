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
#include <memory>

namespace trun {
//
// TestModule is a collection of testable functions
//  test_<library>_<case>
//
    class TestModule {
    public:
        using Ref = std::shared_ptr<TestModule>;
    public:
        static TestModule::Ref Create(const std::string &moduleName);
        TestModule(const std::string &moduleName);
        virtual ~TestModule() = default;

        __inline bool Executed() const { return bExecuted; }

        bool ShouldExecute() const;

        TestFunc::Ref TestCaseFromName(const std::string &caseName) const;
        void SetDependencyForCase(const char *caseName, const char *dependencyList);

        const std::vector<TestFunc::Ref> Dependencies() const {
            return dependencies;
        }
        bool HaveDependency(const TestFunc::Ref &func) const;

        void ResolveDependencies();
        size_t ResolveDependenciesForTest(std::vector<TestFunc::Ref> &outDeps, const TestFunc::Ref &testFunc) const;
    private:
    public:
        std::string name;
        bool bExecuted = false;
        TestFunc::Ref mainFunc = nullptr;
        TestFunc::Ref exitFunc = nullptr;

        TRUN_PRE_POST_HOOK_DELEGATE *cbPreHook = nullptr;
        TRUN_PRE_POST_HOOK_DELEGATE *cbPostHook = nullptr;

        std::vector<TestFunc::Ref> dependencies;
        std::vector<TestFunc::Ref> testFuncs;
    };
}