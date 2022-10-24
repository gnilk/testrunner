#pragma once
#include "platform.h"

#include "dynlib.h"
#include "config.h"
#include "logger.h"
#include "testresult.h"
#include "responseproxy.h"
#include "testinterface.h"
#include "testhooks.h"
#include <string>



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
    void ExecuteAsync();
    bool ShouldExecute();
    bool CheckDependenciesExecuted();

    void SetTestModule(TestModule *_testModule) { testModule = _testModule; }
    void SetLibrary(IDynLibrary *dynLibrary) { library = dynLibrary; }
    TestModule *GetTestModule() { return testModule; }
    const IDynLibrary *Library() const { return library; }

    void SetDependencyList(const char *dependencyList);

    void SetTestScope(kTestScope scope) {
        testScope = scope;
    }
    const TestResult *Result() const { return testResult; }
    kTestScope TestScope() { return testScope; }
public:
    std::string symbolName;
    std::string moduleName;
    std::string caseName;
private:
    kTestScope testScope;
    bool isExecuted;
    gnilk::ILogger *pLogger;
    void HandleTestReturnCode();

    IDynLibrary *library;
    TestModule *testModule;
    TestResponseProxy *trp;

    std::vector<std::string> dependencies;

    PTESTFUNC pFunc;
    int testReturnCode;
    TestResult *testResult;

};


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
    bool Executed() { return bExecuted; }
    bool ShouldExecute() {
        for (auto m:Config::Instance()->modules) {
            if ((m == "-") || (m == name)) {
                return true;
            }
        }
        return false;
    }

    TestFunc *TestCaseFromName(const std::string &caseName) {
        for (auto func : testFuncs) {
            if (func->caseName == caseName) {
                return func;
            }
        }
        return nullptr;
    }

    void SetDependencyForCase(const char *caseName, const char *dependencyList) {
        std::string strName(caseName);
        for(auto func : testFuncs) {
            if (func->caseName == caseName) {
                func->SetDependencyList(dependencyList);
            }
        }
    }

public:
    std::string name;
    bool bExecuted;
    TestFunc *mainFunc;
    TestFunc *exitFunc;

    TRUN_PRE_POST_HOOK_DELEGATE *cbPreHook;
    TRUN_PRE_POST_HOOK_DELEGATE *cbPostHook;

    std::vector<TestFunc *> testFuncs;
};
