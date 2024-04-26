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
#include <unordered_map>

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
        TestResponseProxy &GetTestResponseProxy() {
            return testResponseProxy;
        }

        TestResult::Ref ExecuteTests(IDynLibrary::Ref dynlib);
        TestResult::Ref ExecuteMain(IDynLibrary::Ref dynlib);
        TestResult::Ref ExecuteExit(IDynLibrary::Ref dynlib);

        void AddDependencyForCase(const std::string &caseName, const std::string &dependencyList);

    private:
        std::unordered_map<std::string, std::vector<std::string>> caseDependencyList;
    public:
        std::string name;
        bool bExecuted = false;
        TestFunc::Ref mainFunc = nullptr;
        TestFunc::Ref exitFunc = nullptr;
        TestResponseProxy testResponseProxy;

        TRUN_PRE_POST_HOOK_DELEGATE *cbPreHook = nullptr;
        TRUN_PRE_POST_HOOK_DELEGATE *cbPostHook = nullptr;

        std::vector<TestFunc::Ref> dependencies;
        std::vector<TestFunc::Ref> testFuncs;
    };
}