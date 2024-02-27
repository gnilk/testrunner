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

#ifdef WIN32
#include <Windows.h>
#else
    #if TRUN_HAVE_THREADS
        #include <pthread.h>
        #include <thread>
    #endif
#endif
#include <map>
#include "testfunc.h"
#include "config.h"
#include "testrunner.h"
#include "testmodule.h"
#include "responseproxy.h"
#include "strutil.h"
#include <string>

using namespace trun;


TestFunc::TestFunc() {
    isExecuted = false;
    pLogger = Logger::GetLogger("TestFunc");
}

TestFunc::TestFunc(std::string symbolName, std::string moduleName, std::string caseName) {
    this->symbolName = symbolName;
    this->moduleName = moduleName;
    this->caseName = caseName;
    testScope = kTestScope::kUnknown;
    isExecuted = false;
    pLogger = Logger::GetLogger("TestFunc");
    testResult = nullptr;
    testReturnCode = -1;
}

bool TestFunc::IsGlobal() {
    return (moduleName == "-");
}
bool TestFunc::IsGlobalMain() {
    return (IsGlobal() && (caseName == Config::Instance()->mainFuncName));
}
bool TestFunc::IsGlobalExit() {
    return (IsGlobal() && (caseName == Config::Instance()->exitFuncName));
}
bool TestFunc::IsModuleMain() { {
    return (IsGlobal() && (caseName == Config::Instance()->mainFuncName));
}}

bool TestFunc::IsModuleExit() {
    return (!IsGlobal() && (caseName == Config::Instance()->exitFuncName));
}
//__inline__ static void trap_instruction(void)
//{
//    __asm__ volatile("int $0x03");
//}

/*
bool TestFunc::ShouldExecute() {
    if (this->isExecuted) {
        return false;
    }
    if ((testScope == kTestScope::kModuleMain) || (testScope == kTestScope::kModuleExit)) {
        return Config::Instance()->testModuleGlobals;
    }

    if (caseMatch(caseName, Config::Instance()->testcases)) {
        //return CheckDependenciesExecuted();
        return true;
    }
    return false;
}
*/

bool TestFunc::ShouldExecuteNoDeps() {
    if (this->isExecuted) {
        return false;
    }
    if ((testScope == kTestScope::kModuleMain) || (testScope == kTestScope::kModuleExit)) {
        return Config::Instance()->testModuleGlobals;
    }

    return caseMatch(caseName, Config::Instance()->testcases);
}

/*
bool TestFunc::CheckDependenciesExecuted() {
    // Check dependencies
    for (auto depName : dependencies) {
        //auto depFun = testModule->TestCaseFromName(depName);
        TestFunc *depFun = nullptr;
        if ((depFun == nullptr) || (depFun == this)) {
            printf("WARNING: Can't depend on yourself!!!!!\n");
            continue;
        }

        if (!depFun->isExecuted) {
            if (!depFun->ShouldExecute()) {
                pLogger->Warning("Case '%s' has dependency '%s' that won't be executed!!!", caseName.c_str(), depFun->caseName.c_str());
            }
            return false;
        }
    }
    return true;
}
 */


void TestFunc::ExecuteSync() {
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
    func->ExecuteSync();
    return NULL;
}

void TestFunc::ExecuteAsync() {
    DWORD dwThreadID;
    HANDLE hThread = CreateThread(NULL, 0, testfunc_thread_starter, this, 0, &dwThreadID);
    pLogger->Debug("Execute, waiting for thread");
    WaitForSingleObject(hThread, INFINITE);
}

#else
// Pthread wrapper..
#ifdef TRUN_HAVE_THREADS
static void *testfunc_thread_starter(void *arg) {
    TestFunc *func = reinterpret_cast<TestFunc*>(arg);
    func->ExecuteSync();
    // Return NULL here as this is a C callback..
    return NULL;
}

void TestFunc::ExecuteAsync() {
        pthread_attr_t attr;
        pthread_t hThread;
        int err;
        pthread_attr_init(&attr);

        {
            int s;
            size_t v;
            s = pthread_attr_getstacksize(&attr, &v);
            if (s) {
                pLogger->Error("Failed to fetch stack size: %d\n", s);
            } else {
                pLogger->Debug("Thread Stack size = %d kbytes\n", v/1024);

            }
        }

        if ((err = pthread_create(&hThread,&attr,testfunc_thread_starter, this))) {
            pLogger->Error("pthread_create, failed with code: %d\n", err);
            exit(1);
        }
        pLogger->Debug("Execute, waiting for thread");
        void *ret;
        pthread_join(hThread, &ret);
}
#endif
#endif

TestResult::Ref TestFunc::Execute(IDynLibrary *module) {
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
    if (pFunc != nullptr) {
        //
        // Actual execution of test function and handling of result
        //
        // Execute the test in it's own thread.
        // This allows the test to be aborted when the response proxy is called
        testResult = TestResult::Create(symbolName);

#if defined(TRUN_HAVE_THREADS)
        ExecuteAsync();
        pLogger->Debug("Execute, thread done...\n");
#else
        ExecuteSync();
#endif

        trp->End();
        testResult->SetAssertError(trp->GetAssertError());
        testResult->SetResult(trp->Result());
        testResult->SetNumberOfErrors(trp->Errors());
        testResult->SetNumberOfAsserts(trp->Asserts());
        testResult->SetTimeElapsedSec(trp->ElapsedTimeInSec());
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
        default:
            if ((testResult->Errors() > 0) || (testResult->Asserts() > 0)) {
                // in this case we have called 'thread_exit' and we don't have a return code..
                testResult->SetResult(kTestResult_TestFail);
            } else {
                // This could be depending on 'strict' checking flag (fail in strict mode)
                testResult->SetResult(kTestResult_InvalidReturnCode);
            }

    }
}

void TestFunc::SetExecuted() {
    isExecuted = true;
}

bool TestFunc::Executed() {
    return isExecuted;
}


void TestFunc::SetDependencyList(const char *dependencyList) {
    pLogger->Debug("Setting dependency for '%s' (%s) to: %s\n", caseName.c_str(), symbolName.c_str(), dependencyList);
    trun::split(dependencies, dependencyList, ',');
}
