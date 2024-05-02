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

        bool ShouldExecute() const;
        __inline bool IsIdle() const {
            return state == kState::Idle;
        }


        TestFunc::Ref TestCaseFromName(const std::string &caseName) const;
        TestResponseProxy &GetTestResponseProxy() {
            return testResponseProxy;
        }



        TestResult::Ref Execute(IDynLibrary::Ref dynlib);
        TestResult::Ref ExecuteMain(IDynLibrary::Ref dynlib);
        TestResult::Ref ExecuteExit(IDynLibrary::Ref dynlib);

        void AddDependency(TestModule::Ref depModule);
        const std::vector<TestModule::Ref> &Dependencies() const { return dependencies; }

        void AddDependencyForCase(const std::string &caseName, const std::string &dependencyList);

    protected:
        enum class kState {
            Idle,
            Executing,
            Finished,
        };
        kState state = kState::Idle;

        kState State() {
            return state;
        }

        void ChangeState(kState newState) {
            state = newState;
        }

        void ExecuteDependencies(IDynLibrary::Ref dynlib);

        // Internal - this wraps state handling...
        TestResult::Ref DoExecute(IDynLibrary::Ref dynlib);
        TestResult::Ref DoExecuteFunc(IDynLibrary::Ref dynlib, TestFunc::Ref func);

        std::unordered_map<std::string, std::vector<std::string>> caseDependencyList;
    public: // FIX THIS - I don't want these public...
        std::string name;
        std::vector<TestModule::Ref> dependencies;

        TestFunc::Ref mainFunc = nullptr;
        TestFunc::Ref exitFunc = nullptr;
        TestResponseProxy testResponseProxy;

        TRUN_PRE_POST_HOOK_DELEGATE *cbPreHook = nullptr;
        TRUN_PRE_POST_HOOK_DELEGATE *cbPostHook = nullptr;

        std::vector<TestFunc::Ref> testFuncs;
    };
}