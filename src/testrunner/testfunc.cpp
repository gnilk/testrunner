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
 - 2020.01.10, FKling, Execution of tests now runs in own thread
 - 2018.10.18, FKling, Implementation

 
 ---------------------------------------------------------------------------*/

#include "testfunc.h"
#include "module_mac.h"
#include "config.h"
#include "testrunner.h"
#include "responseproxy.h"
#include <string>
#ifdef WIN32
#include <Windows.h>
#else
#include <pthread.h>
#include <thread>
#endif
#include <map>


TestFunc::TestFunc() {
    isExecuted = false;
    pLogger = gnilk::Logger::GetLogger("TestFunc");
    testModule = NULL;
}

TestFunc::TestFunc(std::string symbolName, std::string moduleName, std::string caseName) {
    this->symbolName = symbolName;
    this->moduleName = moduleName;
    this->caseName = caseName;
    isExecuted = false;
    pLogger = gnilk::Logger::GetLogger("TestFunc");
    testResult = NULL;
    testReturnCode = -1;
    testModule = NULL;
}

bool TestFunc::IsGlobal() {
    return (moduleName == "-");
}
bool TestFunc::IsGlobalMain() {
    return (IsGlobal() && (caseName == Config::Instance()->testMain));
}
void TestFunc::ExecuteAsync() {
    // 1) Setup test response proxy
    // This is currently a global instance - not good!

    trp = TestResponseProxy::GetInstance();
    // Begin test
    trp->Begin(symbolName, moduleName);
    // Actual call to test function (in shared lib)
    testReturnCode = pFunc((void *)trp->Proxy());
}

#ifdef WIN32
DWORD WINAPI testfunc_thread_starter(LPVOID lpParam) {
    TestFunc* func = reinterpret_cast<TestFunc*>(lpParam);
    func->ExecuteAsync();
    return NULL;
}

#else
// Pthread wrapper..
static void *testfunc_thread_starter(void *arg) {
    printf("test_func_thread_started, arg is: %p\n", arg);
    TestFunc *func = reinterpret_cast<TestFunc*>(arg);
    func->ExecuteAsync();
    return NULL;
}
#endif

TestResult *TestFunc::Execute(IModule *module) {
    pLogger->Debug("Executing test: %s", caseName.c_str());
    pLogger->Debug("  Module: %s", moduleName.c_str());
    pLogger->Debug("  Case..: %s", caseName.c_str());
    pLogger->Debug("  Export: %s", symbolName.c_str());
    pLogger->Debug("  Func..: %p", this);


    //kTestResult testResult = kTestResult_NotExecuted;

    // Executed?, issue warning, but proceed..
    if (Executed()) {
        pLogger->Warning("Test '%s' already executed - double execution is either bug or not advised!!", symbolName.c_str());
    }
    pFunc = (PTESTFUNC)module->FindExportedSymbol(symbolName);
    if (pFunc != NULL) {
        //
        // Actual execution of test function and handling of result
        //

        // Execute the test in it's own thread.
        // This allows the test to be aborted when the response proxy is called
        testResult = new TestResult(symbolName);
#ifdef WIN32
        DWORD dwThreadID;
        HANDLE hThread = CreateThread(NULL, 0, testfunc_thread_starter, this, 0, &dwThreadID);
        pLogger->Debug("Execute, waiting for thread");
        WaitForSingleObject(hThread, INFINITE);
#else
        pthread_attr_t attr;
        pthread_t hThread;
        int err;
        pthread_attr_init(&attr);
        if ((err = pthread_create(&hThread,&attr,testfunc_thread_starter, this))) {
            pLogger->Error("pthread_create, failed with code: %d\n", err);
            exit(1);
        }
        pLogger->Debug("Execute, waiting for thread");
        void *ret;
        pthread_join(hThread, &ret);
#endif
        pLogger->Debug("Execute, thread done...\n");

        trp->End();
        testResult->SetResult(trp->Result());
        testResult->SetNumberOfErrors(trp->Errors());
        testResult->SetNumberOfAsserts(trp->Asserts());
        testResult->SetTimeElapsedSec(trp->ElapsedTimeInSec());


        // Overwrite the result based on return code
        HandleTestReturnCode();
    } else {
        pLogger->Error("Execute, unable to find exported symbol '%s' for case: %s\n", symbolName.c_str(), caseName.c_str());
    }
    SetExecuted();

    return testResult;
}
void TestFunc::HandleTestReturnCode() {
    // Discard return code???
    if (Config::Instance()->discardTestReturnCode) {
        pLogger->Debug("Discarding return code\n");
        return;
    }
    // Let's overwrite test result from the test
    switch(testReturnCode) {
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
