#pragma once
#include "platform.h"

#include "dynlib.h"
#include "config.h"
#include "logger.h"
#include "testresult.h"
#include "responseproxy.h"
#include "testinterface_internal.h"
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
        enum class kState {
            Idle,
            Executing,
            Finished,
        };

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

        void AddTestFunc(const TestFunc::Ref &tfunc);


        TestResult::Ref Execute(const IDynLibrary::Ref &dynlib);
        TestResult::Ref ExecuteMain(const IDynLibrary::Ref &dynlib);
        TestResult::Ref ExecuteExit(const IDynLibrary::Ref &dynlib);

        void AddDependency(TestModule::Ref depModule);
        const std::vector<TestModule::Ref> &Dependencies() const { return dependencies; }

        void AddDependencyForCase(const std::string &caseName, const std::string &dependencyList) const;

        // Public since we might need to call this when using external execution (fork)
        void ChangeState(kState newState) {
            state = newState;
        }


    protected:
        kState state = kState::Idle;

        kState State() const {
            return state;
        }


        void ExecuteDependencies(const IDynLibrary::Ref &dynlib);

        // Internal - this wraps state handling...
        TestResult::Ref DoExecute(const IDynLibrary::Ref &dynlib);
        TestResult::Ref DoExecuteTestCase(const IDynLibrary::Ref &dynlib, const TestFunc::Ref &func) const;

        std::unordered_map<std::string, std::vector<std::string>> caseDependencyList;
    public: // FIX THIS - I don't want these public...
        std::string name;
        std::vector<TestModule::Ref> dependencies;

        TestFunc::Ref mainFunc = nullptr;
        TestFunc::Ref exitFunc = nullptr;
        TestResponseProxy testResponseProxy;

        CBPrePostHook cbPreHook = {};
        CBPrePostHook cbPostHook = {};

        std::vector<TestFunc::Ref> testFuncs;
    };
}