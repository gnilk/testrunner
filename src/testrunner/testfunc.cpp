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
#include "resultsummary.h"
#include <string>

using namespace trun;

TestFunc::Ref TestFunc::Create(const std::string &use_symbolName, const std::string &use_moduleName, const std::string &use_caseName) {
    return std::make_shared<TestFunc>(use_symbolName, use_moduleName, use_caseName);
}

// Default CTOR only used for unit testing
TestFunc::TestFunc() {
    pLogger = gnilk::Logger::GetLogger("TestFunc");
}

TestFunc::TestFunc(const std::string &use_symbolName, const std::string &use_moduleName, const std::string &use_caseName) {
    symbolName = use_symbolName;
    moduleName = use_moduleName;
    caseName = use_caseName;
    testScope = kTestScope::kUnknown;
    pLogger = gnilk::Logger::GetLogger("TestFunc");
    testResult = nullptr;
    testReturnCode = -1;
}

void TestFunc::SetResultFromPrePostExec(TestResult::Ref newResult) {
    testResult = newResult;
}


bool TestFunc::IsGlobal() {
    return (moduleName == "-");
}
bool TestFunc::IsGlobalMain() {
    return (IsGlobal() && (caseName == Config::Instance().mainFuncName));
}
bool TestFunc::IsGlobalExit() {
    return (IsGlobal() && (caseName == Config::Instance().exitFuncName));
}
bool TestFunc::IsModuleMain() { {
    // FIXME: This is wrong.
    return (IsGlobal() && (caseName == Config::Instance().mainFuncName));
}}

bool TestFunc::IsModuleExit() {
    return (!IsGlobal() && (caseName == Config::Instance().exitFuncName));
}

bool TestFunc::ShouldExecuteNoDeps() {
    // Unless Idle - the rest doesn't matter...
    if (State() != kState::Idle) {
        return false;
    }
    if ((testScope == kTestScope::kModuleMain) || (testScope == kTestScope::kModuleExit)) {
        return Config::Instance().testModuleGlobals;
    }

    return caseMatch(caseName, Config::Instance().testcases);
}

struct ThreadArg {
    TestFunc *testFunc;
    TestModule::Ref testModule;
    TRUN_PRE_POST_HOOK_DELEGATE *cbPreHook;
    TRUN_PRE_POST_HOOK_DELEGATE *cbPostHook;
};


TestResult::Ref TestFunc::Execute(IDynLibrary::Ref dynlib, TRUN_PRE_POST_HOOK_DELEGATE cbPreHook, TRUN_PRE_POST_HOOK_DELEGATE cbPostHook) {
    // Unless idle, we should not execute
    if (State() != kState::Idle) {
        return nullptr;
    }

    pLogger->Debug("Executing test: %s", caseName.c_str());
    pLogger->Debug("  Module: %s", moduleName.c_str());
    pLogger->Debug("  Case..: %s", caseName.c_str());
    pLogger->Debug("  Export: %s", symbolName.c_str());
    pLogger->Debug("  Func..: %p", (void *)this);
    pFunc = (PTESTFUNC)dynlib->FindExportedSymbol(symbolName);
    if (pFunc == nullptr) {
        ChangeState(kState::Finished);
        return nullptr;
    }

    // Without the test-module (as a thread_local instance - we won't execute)
    auto currentModule = TestRunner::HACK_GetCurrentTestModule();
    if (currentModule == nullptr) {
        pLogger->Error("No module, can't execute!");
        ChangeState(kState::Finished);
        return nullptr;
    }

    ChangeState(kState::Executing);
    ExecuteDependencies(dynlib, cbPreHook, cbPostHook);

    printf("=== RUN  \t%s\n",symbolName.c_str());

    auto &proxy = currentModule->GetTestResponseProxy();
    proxy.Begin(symbolName, moduleName);


#if defined(TRUN_HAVE_THREADS)
    if (Config::Instance().enableThreadTestExecution == true) {
        ExecuteAsync(cbPreHook, cbPostHook);
        pLogger->Debug("Execute, thread done...\n");
    } else {
        ExecuteSync(cbPreHook, cbPostHook);
    }
#else
    // If we don't have threads (i.e. embedded) - we simply just execute this...
    ExecuteSync();
#endif
    proxy.End();
    CreateTestResult(proxy);

    PrintTestResult();
    ChangeState(kState::Finished);
    return testResult;
}



#ifdef WIN32
DWORD WINAPI testfunc_thread_starter(LPVOID lpParam) {
    // NOTE: Should not be 'TestFunc::Ref' - called with 'this' as the 'void *' param from within 'TestFunc'
    TestFunc* func = reinterpret_cast<TestFunc *>(lpParam);
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
    auto threadArg = reinterpret_cast<ThreadArg *>(arg);

    TestRunner::HACK_SetCurrentTestModule(threadArg->testModule);
    threadArg->testFunc->ExecuteSync(threadArg->cbPreHook, threadArg->cbPostHook);
    // Return NULL here as this is a C callback..
    return NULL;
}

void TestFunc::ExecuteAsync(TRUN_PRE_POST_HOOK_DELEGATE cbPreHook, TRUN_PRE_POST_HOOK_DELEGATE cbPostHook) {
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

    auto threadArg = ThreadArg {
            .testFunc = this,
            .testModule = TestRunner::HACK_GetCurrentTestModule(),
            .cbPreHook = cbPreHook,
            .cbPostHook = cbPostHook,
    };

    if ((err = pthread_create(&hThread,&attr,testfunc_thread_starter, &threadArg))) {
        pLogger->Error("pthread_create, failed with code: %d\n", err);
        exit(1);
    }
    pLogger->Debug("Execute, waiting for thread");
    void *ret;
    pthread_join(hThread, &ret);
}
#endif
#endif

void TestFunc::ExecuteSync(TRUN_PRE_POST_HOOK_DELEGATE cbPreHook, TRUN_PRE_POST_HOOK_DELEGATE cbPostHook) {

    // Begin test, note: THIS MUST BE DONE HERE - in case of threading!
    auto currentModule = TestRunner::HACK_GetCurrentTestModule();
    if (currentModule == nullptr) {
        pLogger->Error("No module, can't execute!");
        return;
    }

    auto &proxy = currentModule->GetTestResponseProxy();

    if (cbPreHook != nullptr) {
        // Note: in case of threaded test execution, we this will terminate on error - we need to disable thread here OR we need to actually thread pre/post as well
        int preHookReturnCode = cbPreHook(proxy.GetExtInterface());
        if (preHookReturnCode != kTR_Pass) {
            testReturnCode = preHookReturnCode;
            return;
        }
    }

    // Main
    testReturnCode = pFunc((void *) proxy.GetExtInterface());

    if (cbPostHook != nullptr) {
        int postHookReturnCode = cbPostHook(proxy.GetExtInterface());
        if (postHookReturnCode != kTR_Pass) {
            // FIXME: We need a flag to deal with
            testReturnCode = postHookReturnCode;
        }
    }
}


void TestFunc::CreateTestResult(TestResponseProxy &proxy) {

    testResult = TestResult::Create(symbolName);
    testResult->SetAssertError(proxy.GetAssertError());
    testResult->SetResult(proxy.Result());
    testResult->SetNumberOfErrors(proxy.Errors());
    testResult->SetNumberOfAsserts(proxy.Asserts());
    testResult->SetTimeElapsedSec(proxy.ElapsedTimeInSec());

    // Should be done last..
    testResult->SetTestResultFromReturnCode(testReturnCode);
}

void TestFunc::PrintTestResult() {
    double tElapsedSec = testResult->ElapsedTimeSec();
    if (testResult->Result() != kTestResult_Pass) {
        if (testResult->Result() == kTestResult_InvalidReturnCode) {

            printf("=== INVALID RETURN CODE (%d) for %s", testResult->Result(), testResult->SymbolName().c_str());
        } else {
            //std::string failState = testResult->FailState() == TestResult::kFailState::PreHook?"pre-hook"
            if (testResult->FailState() == TestResult::kFailState::Main) {
                printf("=== FAIL:\t%s, %.3f sec, %d, %d, %d\n", testResult->SymbolName().c_str(), tElapsedSec,
                       testResult->Result(), testResult->Errors(), testResult->Asserts());
            } else {
                printf("=== FAIL:\t%s (%s), %.3f sec, %d, %d, %d\n",
                       testResult->SymbolName().c_str(),
                       testResult->FailStateName().c_str(),
                       tElapsedSec,
                       testResult->Result(), testResult->Errors(), testResult->Asserts());
            }
        }
    } else {
        if ((testResult->Errors() != 0) || (testResult->Asserts() != 0)) {
            printf("=== FAIL:\t%s, %.3f sec, %d, %d, %d\n",testResult->SymbolName().c_str(), tElapsedSec, testResult->Result(), testResult->Errors(), testResult->Asserts());
        } else {
            printf("=== PASS:\t%s, %.3f sec, %d\n",testResult->SymbolName().c_str(),tElapsedSec, testResult->Result());
        }
    }
    printf("\n");
}


void TestFunc::ExecuteDependencies(IDynLibrary::Ref dynlib, TRUN_PRE_POST_HOOK_DELEGATE cbPreHook, TRUN_PRE_POST_HOOK_DELEGATE cbPostHook) {
    for (auto &func : dependencies) {
        if (!func->IsIdle()) {
            continue;
        }
        Execute(dynlib, cbPreHook, cbPostHook);
    }
}

void TestFunc::AddDependency(TestFunc::Ref depCase) {
    pLogger->Debug("Add '%s' as dependency for '%s' (%s)", depCase->symbolName.c_str(), caseName.c_str(), symbolName.c_str());
    dependencies.push_back(depCase);
}

