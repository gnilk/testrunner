#pragma once
#include "platform.h"

#include "dynlib.h"
#include "config.h"
#include "logger.h"
#include "testresult.h"
#include "responseproxy.h"
#include "testinterface.h"
#include "testhooks.h"
#include "strutil.h"
#include <string>

namespace trun {

    class TestFunc;
    class TestModule;

// The core structure defining a testable function which belongs to a test-library
    class TestFunc {
    public:
        enum class kTestScope {
            kUnknown,
            kGlobal,
            kModuleMain,
            kModuleExit,
            kModuleCase,
        };
    public:
        TestFunc();
        TestFunc(std::string symbolName, std::string moduleName, std::string caseName);
        bool IsGlobal();
        bool IsModuleExit();
        bool IsModuleMain();
        bool IsGlobalMain();
        bool IsGlobalExit();
        TestResult::Ref Execute(IDynLibrary *module);
        void SetExecuted();
        bool Executed();
        bool ShouldExecute();
        bool ShouldExecuteNoDeps();
        //bool CheckDependenciesExecuted();

        // FIXME: these two classes need to reference each other...
        //void SetTestModule(TestModule::Ref newTestModule) { testModule = newTestModule; }
        //TestModule *GetTestModule() { return testModule; }

        void SetLibrary(IDynLibrary *dynLibrary) { library = dynLibrary; }
        const IDynLibrary *Library() const { return library; }

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
#ifdef WIN32
            // Windows has no conditional compile - so always declare
            void ExecuteAsync();
#else
#ifdef TRUN_HAVE_THREADS
            void ExecuteAsync();
#endif
#endif
    public:
        std::string symbolName;
        std::string moduleName;
        std::string caseName;
    private:
        void HandleTestReturnCode();

        kTestScope testScope = kTestScope::kUnknown;
        bool isExecuted = false;
        ILogger *pLogger = nullptr;

        IDynLibrary *library = nullptr;

        // This is strictly only needed to resolve dependencies
        //TestModule::Ref testModule = nullptr;
        TestResponseProxy *trp = nullptr;

        std::vector<std::string> dependencies;

        PTESTFUNC pFunc = nullptr;
        int testReturnCode = -1;
        TestResult::Ref testResult = nullptr;

    };
}