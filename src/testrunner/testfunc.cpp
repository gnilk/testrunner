/*-------------------------------------------------------------------------
 File    : testfunc.cpp
 Author  : FKling
 Version : -
 Orginal : 2018-10-18
 Descr   : Wrapper for a function to be tested

 Encapsulates the external function pointer. Responsible for calling this function
 and gather the results. Handling of test case result code transformation is done here.
 
 Part of testrunner
 BSD3 License!
 
 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>

 </pre>
 
 
 \History
 - 2018.10.18, FKling, Implementation
 
 ---------------------------------------------------------------------------*/

#include "testfunc.h"
#include "module.h"
#include "config.h"
#include "testrunner.h"
#include "responseproxy.h"
#include <string>
#ifdef WIN32
#else
#include <pthread.h>
#endif
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
TestResult *TestFunc::Execute(IModule *module) {
    pLogger->Debug("Executing test: %s", caseName.c_str());
    pLogger->Debug("  Module: %s", moduleName.c_str());
    pLogger->Debug("  Case..: %s", caseName.c_str());
    pLogger->Debug("  Export: %s", symbolName.c_str());


    //kTestResult testResult = kTestResult_NotExecuted;
    TestResult *testResult = new TestResult(symbolName);

    // Executed?, issue warning, but proceed..
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
        trp->Begin(symbolName, moduleName);

        // 2) call function to be tested
        int testReturnCode = pFunc((void *)trp->Proxy());
        // 3) Stop test
        trp->End();

        // 4) Gather data from test
        testResult->SetResult(trp->Result());
        testResult->SetNumberOfErrors(trp->Errors());
        testResult->SetNumberOfAsserts(trp->Asserts());
        testResult->SetTimeElapsedSec(trp->ElapsedTimeInSec());
        
        // Overwrite the result based on return code
        HandleTestReturnCode(testReturnCode, testResult);
    }
    SetExecuted();

    return testResult;
}
void TestFunc::HandleTestReturnCode(int code, TestResult *testResult) {
    // Discard return code???
    if (Config::Instance()->discardTestReturnCode) {
        pLogger->Debug("Discarding return code\n");
        return;
    }
    // Let's overwrite test result from the test
    switch(code) {
        case kTR_Pass :
            if ((testResult->Errors() == 0) && (testResult->Asserts() == 0)) {
                testResult->SetResult(kTestResult_Pass);
            } else {
                // This could be depending on 'strict' checking flag (fail in strict mode)
                testResult->SetResult(kTestResult_TestFail);
            }
            break;
        case kTR_Fail :
            testResult->SetResult(kTestResult_TestFail);
            break;
        case kTR_FailModule :
            testResult->SetResult(kTestResult_ModuleFail);
            break;
        case kTR_FailAll :
            testResult->SetResult(kTestResult_AllFail);
            break;
    }
}

void TestFunc::SetExecuted() {
    isExecuted = true;
}

bool TestFunc::Executed() {
    return isExecuted;
}
