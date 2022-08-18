#include "../testinterface.h"
#include "../testfunc.h"
#include "../module.h"
#include <vector>
#include <string>

extern "C" {
DLL_EXPORT int test_tfunc(ITesting *t);
DLL_EXPORT int test_tfunc_globals(ITesting *t);
DLL_EXPORT int test_tfunc_exec(ITesting *t);
}

static int test_mock_func(ITesting *t) {
    printf("INTERNAL MOCKED TEST FUNCTION!\n");
    return kTR_Pass;
}

// This mock's a module loader and with a single function the 'test_mock_func'..
class ModuleMock : public IModule {
public:
    ModuleMock() = default;
    virtual ~ModuleMock() = default;

    void *Handle() override {
        return nullptr;
    }
    void *FindExportedSymbol(std::string funcName) override {
        return (void *) test_mock_func;
    }
    std::vector<std::string> &Exports() override {
        static std::vector<string> dummy={"test_mock_func"};
        return dummy;
    }
    bool Scan(std::string pathName) override {
        return true;
    }
    std::string &Name() override {
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
    ModuleMock mockModule;
    TestFunc func("test_mock_func", "mock", "func");
    TR_ASSERT(t, !func.IsGlobal());
    TR_ASSERT(t, !func.IsGlobalMain());
    TR_ASSERT(t, !func.IsGlobalExit());
    TR_ASSERT(t, !func.IsModuleExit());

    //
    // This will actually test quite a bunch of things - more like an integration test as the TestFunc is fairly
    // high-level. We are testing: TestResponseProxy, AssertError, TestResult, etc...
    //
    auto testResult = func.Execute(&mockModule);
    TR_ASSERT(t, testResult != nullptr);

    TR_ASSERT(t, testResult->Errors() == 0);
    TR_ASSERT(t, testResult->Result() == kTestResult_Pass);


    return kTR_Pass;
}
