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
        typedef enum {
            kUnknown,
            kGlobal,
            kModuleMain,
            kModuleExit,
            kModuleCase,
        } kTestScope;
    public:
        TestFunc();
        TestFunc(std::string symbolName, std::string moduleName, std::string caseName);
        bool IsGlobal();
        bool IsModuleExit();
        bool IsModuleMain();
        bool IsGlobalMain();
        bool IsGlobalExit();
        TestResult *Execute(IDynLibrary *module);
        void SetExecuted();
        bool Executed();
        bool ShouldExecute();
        bool ShouldExecuteNoDeps();
        bool CheckDependenciesExecuted();

        void SetTestModule(TestModule *_testModule) { testModule = _testModule; }
        void SetLibrary(IDynLibrary *dynLibrary) { library = dynLibrary; }
        TestModule *GetTestModule() { return testModule; }
        const IDynLibrary *Library() const { return library; }

        void SetDependencyList(const char *dependencyList);
        const std::vector<std::string> &Dependencies() const { return dependencies; }

        void SetTestScope(kTestScope scope) {
            testScope = scope;
        }
        const TestResult *Result() const { return testResult; }
        kTestScope TestScope() { return testScope; }

    public:
        // Note: Must be public as we are executing through Win32 Threading layer...
        void ExecuteSync();
    private:
        void ExecuteAsync();
    public:
        std::string symbolName;
        std::string moduleName;
        std::string caseName;
    private:
        kTestScope testScope;
        bool isExecuted;
        ILogger *pLogger;
        void HandleTestReturnCode();

        IDynLibrary *library;
        TestModule *testModule;
        TestResponseProxy *trp;

        std::vector<std::string> dependencies;

        PTESTFUNC pFunc;
        int testReturnCode;
        TestResult *testResult;

    };
}