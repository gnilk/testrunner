#include "../testinterface.h"
#include "../testfunc.h"
#include "../dynlib.h"
#include "../strutil.h"
#include "../testrunner.h"
#include <vector>
#include <string>

using namespace trun;


extern "C" {
    DLL_EXPORT int test_tfunc(ITesting *t);
    DLL_EXPORT int test_tfunc_globals(ITesting *t);
    DLL_EXPORT int test_tfunc_exec(ITesting *t);
    DLL_EXPORT int test_tfunc_casefilter_simple(ITesting *t);
    DLL_EXPORT int test_tfunc_casefilter_splitmid(ITesting *t);
    DLL_EXPORT int test_tfunc_casefilter_trailing(ITesting *t);
    DLL_EXPORT int test_tfunc_modfilter_simple(ITesting *t);
    DLL_EXPORT int test_tfunc_modfilter_trailing(ITesting *t);
    DLL_EXPORT int test_tfunc_casematch_simple(ITesting *t);
    DLL_EXPORT int test_tfunc_empty(ITesting *t);
    DLL_EXPORT int test_tfunc_illegalreturn(ITesting *t);
}


static int test_mock_func(ITesting *t) {
    printf("INTERNAL MOCKED TEST FUNCTION!\n");
    return kTR_Pass;
}

// This mock's a library loader and with a single function the 'test_mock_func'..
class DynlibMock : public IDynLibrary {
public:
    DynlibMock() = default;
    virtual ~DynlibMock() = default;

    void *Handle() override { return nullptr; }
    PTESTFUNC FindExportedSymbol(const std::string &funcName) override {
        return (PTESTFUNC) test_mock_func;
    }
    const std::vector<std::string> &Exports() const override {
        static std::vector<std::string> dummy={"test_mock_func"};
        return dummy;
    }
    bool Scan(const std::string &pathName) override {
        return true;
    }
    const std::string &Name() const override {
        static std::string name = "mockmodule";
        return name;
    }

};

DLL_EXPORT int test_tfunc(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_tfunc_globals(ITesting *t) {
    TestFunc func("test_main", "-", "main");
    TR_ASSERT(t, func.IsGlobal());
    TR_ASSERT(t, func.IsGlobalMain());
    return kTR_Pass;
}

DLL_EXPORT int test_tfunc_exec(ITesting *t) {

    ITestingConfig *itrun_config = nullptr;
    t->QueryInterface(ITestingConfig_IFace_ID, (void **)&itrun_config);
    if (itrun_config == nullptr) {
        return kTR_Fail;
    }
    TRUN_ConfigItem threadedTestExecution = {};
    itrun_config->Get("enableThreadTestExecution", &threadedTestExecution);
    if (!threadedTestExecution.isValid) {
        return kTR_Fail;
    }
    if (threadedTestExecution.value.boolean) {
        printf("FAIL - this test doesn't work in a threaded environment!!!\n");
        return kTR_Fail;
    }

    // Note: This test won't work in threaded mode...
    DynlibMock::Ref mockDynLib = std::make_shared<DynlibMock>();
    TestFunc func("test_mock_func", "mock", "func");
    TR_ASSERT(t, !func.IsGlobal());
    TR_ASSERT(t, !func.IsGlobalMain());
    TR_ASSERT(t, !func.IsGlobalExit());
    TR_ASSERT(t, !func.IsModuleExit());

    //
    // This will actually test quite a bunch of things - more like an integration test as the TestFunc is fairly
    // high-level. We are testing: TestResponseProxy, AssertError, TestResult, etc...
    //

    auto actualRunningModule = TestRunner::HACK_GetCurrentTestModule();
    // Note: I don't think this will be 'null' on windows
    auto mockModule = TestModule::Create("mock");
    TestRunner::HACK_SetCurrentTestModule(mockModule);

    auto testResult = func.Execute(mockDynLib);
    TR_ASSERT(t, testResult != nullptr);

    TR_ASSERT(t, testResult->Errors() == 0);
    TR_ASSERT(t, testResult->Result() == kTestResult_Pass);

    TestRunner::HACK_SetCurrentTestModule(actualRunningModule);

    return kTR_Pass;
}

DLL_EXPORT int test_tfunc_casefilter_simple(ITesting *t) {
    DynlibMock::Ref mockModule = std::make_shared<DynlibMock>();
    auto testCasesOrig = Config::Instance().testcases;

    Config::Instance().testcases = {"func*"};
    TestFunc func("test_mock_func", "mock", "funcflurp");
    TR_ASSERT(t, func.ShouldExecute());

    Config::Instance().testcases = testCasesOrig;
    return kTR_Pass;
}

DLL_EXPORT int test_tfunc_casefilter_splitmid(ITesting *t) {
    DynlibMock::Ref mockModule = std::make_shared<DynlibMock>();
    auto testCasesOrig = Config::Instance().testcases;

    Config::Instance().testcases = {"fn_*_case"};
    TestFunc func("test_mock_func", "mock", "fn_test_case");
    TR_ASSERT(t, func.ShouldExecute());

    Config::Instance().testcases = testCasesOrig;
    return kTR_Pass;
}

DLL_EXPORT int test_tfunc_casefilter_trailing(ITesting *t) {
    DynlibMock::Ref mockModule = std::make_shared<DynlibMock>();
    auto testCasesOrig = Config::Instance().testcases;

    Config::Instance().testcases = {"*flurp"};
    TestFunc func("test_mock_func", "mock", "funcflurp");
    TR_ASSERT(t, func.ShouldExecute());

    Config::Instance().testcases = testCasesOrig;
    return kTR_Pass;
}

DLL_EXPORT int test_tfunc_modfilter_simple(ITesting *t) {
    DynlibMock::Ref mockModule = std::make_shared<DynlibMock>();
    auto modulesOrig = Config::Instance().modules;

    Config::Instance().modules = {"base*"};
    TestFunc func("test_mock_func", "basemod", "func");
    TR_ASSERT(t, func.ShouldExecute());

    Config::Instance().modules = modulesOrig;
    return kTR_Pass;
}

DLL_EXPORT int test_tfunc_modfilter_trailing(ITesting *t) {
    DynlibMock::Ref mockModule = std::make_shared<DynlibMock>();
    auto modulesOrig = Config::Instance().modules;

    Config::Instance().modules = {"*mod"};
    TestFunc func("test_mock_func", "basemod", "func");
    TR_ASSERT(t, func.ShouldExecute());

    Config::Instance().modules = modulesOrig;
    return kTR_Pass;
}


DLL_EXPORT int test_tfunc_casematch_simple(ITesting *t) {
    DynlibMock::Ref mockModule = std::make_shared<DynlibMock>();
    auto testCasesOrig = Config::Instance().testcases;

    Config::Instance().testcases = {"-","caseA","!caseB"};
    TestFunc funcA("test_mock_func", "mock", "caseA");
    TestFunc funcB("test_mock_func", "mock", "caseB");
    TR_ASSERT(t, caseMatch(funcA.CaseName(), Config::Instance().testcases));
    TR_ASSERT(t, !caseMatch(funcB.CaseName(), Config::Instance().testcases));
    //TR_ASSERT(t, func.ShouldExecute());

    Config::Instance().testcases = testCasesOrig;
    return kTR_Pass;

}

DLL_EXPORT int test_tfunc_empty(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_tfunc_illegalreturn(ITesting *t) {
    return -1;
}
