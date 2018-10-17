#include "testfunc.h"
#include "module.h"
#include "config.h"
#include "testrunner.h"
#include "responseproxy.h"
#include <string>
#include <pthread.h>
#include <map>


TestFunc::TestFunc() {
    isExecuted = false;
    pLogger = gnilk::Logger::GetLogger("TestFunc");
}
TestFunc::TestFunc(std::string symbolName, std::string moduleName, std::string caseName) {
    this->symbolName = symbolName;
    this->moduleName = moduleName;
    this->caseName = caseName;
    isExecuted = false;
    pLogger = gnilk::Logger::GetLogger("TestFunc");
}

bool TestFunc::IsGlobal() {
    return (moduleName == "-");
}
bool TestFunc::IsGlobalMain() {
    return (IsGlobal() && (caseName == Config::Instance()->testMain));
}
void TestFunc::Execute(IModule *module) {
    pLogger->Debug("Executing test: %s", caseName.c_str());
    pLogger->Debug("  Module: %s", moduleName.c_str());
    pLogger->Debug("  Case..: %s", caseName.c_str());
    pLogger->Debug("  Export: %s", symbolName.c_str());

    // ?Executed, issue warning, but proceed..
    if (Executed()) {
        pLogger->Warning("Test '%s' already executed - double execution is either bug or not advised!!", symbolName.c_str());
    }

    PTESTFUNC pFunc = (PTESTFUNC)module->FindExportedSymbol(symbolName);

    if (pFunc != NULL) {
        //
        // Actual execution of test function and handling of result
        //

        // 1) Setup test response proxy
        TestResponseProxy *trp = TestResponseProxy::GetInstance();
        trp->Begin(symbolName);
        printf("=== RUN  \t%s\n",symbolName.c_str());

        // 2) call function to be tested
        pFunc((void *)trp->Proxy());

        // 3) Stop test
        trp->End();

        // 4) Gather data from test
        int numError = trp->Errors();
        kTestResult testResult = trp->Result();
        double tElapsedSec = trp->ElapsedTimeInSec();
        if (testResult != kTestResult_Pass) {
            printf("=== FAIL:\t%s (%.3f sec) - %d\n",symbolName.c_str(), tElapsedSec, testResult);
        } else {
            printf("=== PASS:\t%s (%.3f sec)\n",symbolName.c_str(),tElapsedSec);
        }
    }
    SetExecuted();

}

void TestFunc::SetExecuted() {
    isExecuted = true;
}

bool TestFunc::Executed() {
    return isExecuted;
}
