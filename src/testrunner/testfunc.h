#pragma once

#include <string>
#include <memory>

#include "platform.h"

#include "dynlib.h"
#include "config.h"
#include "logger.h"
#include "testresult.h"
#include "responseproxy.h"
#include "testinterface.h"
#include "testhooks.h"
#include "strutil.h"

namespace trun {

    // The core structure defining a testable function which belongs to a test-module
    class TestFunc {
    public:
        using Ref = shared_ptr<TestFunc>;

        enum class kTestScope {
            kUnknown,
            kGlobal,
            kModuleMain,
            kModuleExit,
            kModuleCase,
        };
    public:
        static TestFunc::Ref Create(std::string symbolName, std::string moduleName, std::string caseName);

        TestFunc();
        TestFunc(std::string symbolName, std::string moduleName, std::string caseName);
        bool IsGlobal();
        bool IsModuleExit();
        bool IsModuleMain();
        bool IsGlobalMain();
        bool IsGlobalExit();
        TestResult::Ref Execute(IDynLibrary::Ref module);
        void SetExecuted();
        bool Executed();
        bool ShouldExecute();
        bool ShouldExecuteNoDeps();

        void SetLibrary(IDynLibrary::Ref dynLibrary) { library = dynLibrary; }
        const IDynLibrary::Ref Library() const { return library; }

        void SetDependencyList(const char *dependencyList);
        const std::vector<std::string> &Dependencies() const { return dependencies; }

        void SetTestScope(kTestScope scope) {
            testScope = scope;
        }

        const std::string &SymbolName() const { return symbolName; }
        const std::string &ModuleName() const { return moduleName; }
        const std::string &CaseName() const { return caseName; }
        const TestResult::Ref Result() const { return testResult; }
        kTestScope TestScope() { return testScope; }


        // I use this in the unit test in order to create a mock-up result...
        void UTEST_SetMockResultPtr(TestResult::Ref pMockResult) { testResult = pMockResult; }

    public:
        // Note: Must be public as we are executing through Win32 Threading layer...
        void ExecuteSync();
    private:
        void HandleTestReturnCode();

#ifdef WIN32
            // Windows has no conditional compile - so always declare
            void ExecuteAsync();
#else
#ifdef TRUN_HAVE_THREADS
            void ExecuteAsync();
#endif
#endif
    private:
        std::string symbolName;
        std::string moduleName;
        std::string caseName;

        kTestScope testScope = kTestScope::kUnknown;
        bool isExecuted = false;
        ILogger *pLogger = nullptr;

        IDynLibrary::Ref library = nullptr;

        //TestResponseProxy *trp = nullptr;
        std::vector<std::string> dependencies;

        PTESTFUNC pFunc = nullptr;
        int testReturnCode = -1;
        TestResult::Ref testResult = nullptr;

    };
}