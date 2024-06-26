#pragma once

#include <string>
#include <memory>

#include "platform.h"

#include "dynlib.h"
#include "config.h"
#include "logger.h"
#include "testresult.h"
#include "responseproxy.h"
#include "testinterface_internal.h"
#include "strutil.h"

namespace trun {


    // TEMP - Move out of here!
    class TestFuncExecutorSequential;



    // The core structure defining a testable function which belongs to a test-module
    class TestFunc {
        friend TestFuncExecutorSequential;
    public:
        using Ref = std::shared_ptr<TestFunc>;
        enum class kState {
            Idle,
            Executing,
            Finished,
        };
        enum class kExecState {
            None,
            PreCallback,
            Main,
            PostCallback,
        };
        enum class kTestScope {
            kUnknown,
            kGlobal,
            kModuleMain,
            kModuleExit,
            kModuleCase,
        };
    public:
        static TestFunc::Ref Create(const std::string &use_symbolName, const std::string &use_moduleName, const std::string &use_caseName);

        TestFunc();
        TestFunc(const std::string &use_symbolName, const std::string &use_moduleName, const std::string &use_caseName);
        bool IsGlobal();
        bool IsModuleExit();
        bool IsModuleMain();
        bool IsGlobalMain();
        bool IsGlobalExit();
        TestResult::Ref Execute(IDynLibrary::Ref module, const CBPrePostHook &cbPreHook, const CBPrePostHook &cbPostHook);

        // This was the old function - which also verified dependencies
        __inline bool ShouldExecute() {
            return ShouldExecuteNoDeps();
        }
        bool ShouldExecuteNoDeps();

        void SetLibrary(IDynLibrary::Ref dynLibrary) { library = dynLibrary; }
        const IDynLibrary::Ref Library() const { return library; }

        void AddDependency(TestFunc::Ref depCase);
        const std::vector<TestFunc::Ref> &Dependencies() const { return dependencies; }

        void SetTestScope(kTestScope scope) {
            testScope = scope;
        }

        const std::string &SymbolName() const { return symbolName; }
        const std::string &ModuleName() const { return moduleName; }
        const std::string &CaseName() const { return caseName; }
        const TestResult::Ref Result() const { return testResult; }
        kTestScope TestScope() { return testScope; }

        bool IsIdle() { return (state == kState::Idle); }
        kState State() { return state; }
        kExecState ExecState() { return execState; }

        // I use this in the unit test in order to create a mock-up result...
        void SetResultFromSubProcess(TestResult::Ref pMockResult) { testResult = pMockResult; }

    protected:
        void CreateTestResult(TestResponseProxy &proxy);
        void PrintTestResult();
        void ExecuteDependencies(IDynLibrary::Ref dynlib, const CBPrePostHook &cbPreHook, const CBPrePostHook &cbPostHook);

        void ChangeState(kState newState) {
            state = newState;
        }
        void ChangeExecState(kExecState newExecState) {
            execState = newExecState;
        }
        int InvokeTestCase(TestResponseProxy &proxy) {
            return pFunc((void *)TestResponseProxy::GetTRTestInterface(library->GetVersion()));
        }

    private:
        std::string symbolName;
        std::string moduleName;
        std::string caseName;

        kState state = kState::Idle;
        kExecState execState = kExecState::None;

        kTestScope testScope = kTestScope::kUnknown;
        gnilk::ILogger *pLogger = nullptr;

        IDynLibrary::Ref library = nullptr;

        //TestResponseProxy *trp = nullptr;
        std::vector<TestFunc::Ref> dependencies;

        PTESTFUNC pFunc = nullptr;
        int testReturnCode = -1;    // hmm...
        TestResult::Ref testResult = nullptr;

    };



}