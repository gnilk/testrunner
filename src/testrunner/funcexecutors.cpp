/*-------------------------------------------------------------------------
 File    : funcexecutors.cpp
 Author  : FKling
 Version : -
 Orginal : 2024-05-02
 Descr   : Implementation of different execution 'strategies' for test-cases

 Part of testrunner
 BSD3 License!

 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TO-DO: [ -:Not done, +:In progress, !:Completed]
 <pre>
 </pre>

 \History
 - 2024.05.02, FKling, Implementation
 ---------------------------------------------------------------------------*/
#include "testrunner.h"
#include "funcexecutors.h"

#ifdef TRUN_HAVE_THREADS
    #include <thread>
    #ifdef WIN32
        #include <Windows.h>
    #else
        #include <pthread.h>
    #endif
#endif


using namespace trun;

//
// Factory function for the executor
// Class only for encapsulation...
//
TestFuncExecutorBase &TestFuncExecutorFactory::Create() {
    static TestFuncExecutorSequential sequentialExecutor;
    // Threaded versions not available on embedded libraray
#ifdef TRUN_HAVE_THREADS
    if (Config::Instance().enableThreadTestExecution == true) {
        static TestFuncExecutorParallel parallelExecutor;
        static TestFuncExecutorParallelPThread parallelExecutorPThread;

        // If we allow this - we MUST use the pthread version...
        if (Config::Instance().allowThreadTermination == true) {
            return parallelExecutorPThread;
        }

        return parallelExecutor;

    }
#endif
    return sequentialExecutor;
}



//
// Sequential execution
//
int TestFuncExecutorSequential::Execute(TestFunc *testFunc, TRUN_PRE_POST_HOOK_DELEGATE cbPreHook, TRUN_PRE_POST_HOOK_DELEGATE cbPostHook) {
    // Begin test, note: THIS MUST BE DONE HERE - in case of threading!
    gnilk::ILogger  *pLogger = gnilk::Logger::GetLogger("TestFuncExeSeq");
    auto currentModule = TestRunner::GetCurrentTestModule();
    if (currentModule == nullptr) {
        pLogger->Error("No module, can't execute!");
        return -1;
    }

    auto &proxy = currentModule->GetTestResponseProxy();

    // Test-case pre function
    if (cbPreHook != nullptr) {
        // Note: in case of threaded test execution, we this will terminate on error - we need to disable thread here OR we need to actually thread pre/post as well
        testFunc->ChangeExecState(TestFunc::kExecState::PreCallback);
        int preHookReturnCode = cbPreHook(proxy.GetExtInterface());
        if (preHookReturnCode != kTR_Pass) {
            //testReturnCode = preHookReturnCode;
            return preHookReturnCode;
        }
    }

    // Main (the actual test case)
    testFunc->ChangeExecState(TestFunc::kExecState::Main);
    int testReturnCode = testFunc->InvokeTestCase(proxy);


    // Test-case post function
    if (cbPostHook != nullptr) {
        testFunc->ChangeExecState(TestFunc::kExecState::PostCallback);
        int postHookReturnCode = cbPostHook(proxy.GetExtInterface());
        if (postHookReturnCode != kTR_Pass) {
            testReturnCode = postHookReturnCode;
        }
    }

    return testReturnCode;
}

//
// Threading versions - these are NOT available on embedded (by default)
//

#ifdef TRUN_HAVE_THREADS
//
// Parallel execution, just wraps a call to 'sequential' within a thread!
//
struct ThreadArg {
    TestFunc *testFunc;
    TestModule::Ref testModule;
    TestRunner *testRunner;
    TRUN_PRE_POST_HOOK_DELEGATE *cbPreHook;
    TRUN_PRE_POST_HOOK_DELEGATE *cbPostHook;
    int returnValue;

    // FIXME: Have this disabled by default
    TestFuncExecutorParallelPThread *executor;
};

int TestFuncExecutorParallel::Execute(TestFunc *testFunc, TRUN_PRE_POST_HOOK_DELEGATE cbPreHook, TRUN_PRE_POST_HOOK_DELEGATE cbPostHook) {

    auto threadArg = ThreadArg {
            .testFunc = testFunc,
            .testModule = TestRunner::GetCurrentTestModule(),
            .testRunner = TestRunner::GetCurrentRunner(),
            .cbPreHook = cbPreHook,
            .cbPostHook = cbPostHook,
            .returnValue = -1,
            .executor = nullptr,
    };

    auto thread = std::thread([this, &threadArg] {

        TestRunner::SetCurrentTestModule(threadArg.testModule);
        TestRunner::SetCurrentTestRunner(threadArg.testRunner);

        threadArg.returnValue = TestFuncExecutorSequential::Execute(threadArg.testFunc, threadArg.cbPreHook, threadArg.cbPostHook);
    });

    thread.join();
    return threadArg.returnValue;

}


//
// This is using pthreads for the execution - there is one BIG thing we can do if we use this - the 'TR_ASSERT' macro can terminate the running thread
// and thus it can be used from other places than just the test_case functions...
//
// however - using 'pthread_exit' is NOT very good practice - and there is a reason most newer API's have removed it...
// Windows do support killing it's own thread - I presume they have to unwind the stack somehow - or if they just drop the whole frame..
//
// Pthread wrapper..
#ifdef WIN32
DWORD WINAPI testfunc_thread_starter(LPVOID lpParam) {
    auto threadArg = reinterpret_cast<ThreadArg *>(lpParam);

    TestRunner::SetCurrentTestModule(threadArg->testModule);
    TestRunner::SetCurrentTestRunner(threadArg->testRunner);

    threadArg->returnValue = threadArg->executor->ThreadFunc(threadArg->testFunc, threadArg->cbPreHook, threadArg->cbPostHook);

    return 0;
}
#else
static void *testfunc_thread_starter(void *arg) {
    auto threadArg = reinterpret_cast<ThreadArg *>(arg);

    TestRunner::SetCurrentTestModule(threadArg->testModule);
    TestRunner::SetCurrentTestRunner(threadArg->testRunner);

    threadArg->returnValue = threadArg->executor->ThreadFunc(threadArg->testFunc, threadArg->cbPreHook, threadArg->cbPostHook);
    // Return NULL here as this is a C callback..
    return NULL;
}
#endif


int TestFuncExecutorParallelPThread::ThreadFunc(TestFunc *testFunc, TRUN_PRE_POST_HOOK_DELEGATE cbPreHook, TRUN_PRE_POST_HOOK_DELEGATE cbPostHook) {
    return TestFuncExecutorSequential::Execute(testFunc, cbPreHook, cbPostHook);
}

int TestFuncExecutorParallelPThread::Execute(TestFunc *testFunc, TRUN_PRE_POST_HOOK_DELEGATE cbPreHook, TRUN_PRE_POST_HOOK_DELEGATE cbPostHook) {

    auto threadArg = ThreadArg {
            .testFunc = testFunc,
            .testModule = TestRunner::GetCurrentTestModule(),
            .testRunner = TestRunner::GetCurrentRunner(),
            .cbPreHook = cbPreHook,
            .cbPostHook = cbPostHook,
            .returnValue = -1,
            .executor = this,

    };

#ifdef WIN32
    DWORD dwThreadID;
    HANDLE hThread = CreateThread(NULL, 0, testfunc_thread_starter, &threadArg, 0, &dwThreadID);
    if (hThread == INVALID_HANDLE_VALUE) {
        // FIXME: Some error here would probably be nice
        exit(1);
    }
    WaitForSingleObject(hThread, INFINITE);
#else
    pthread_t hThread;
    if (pthread_create(&hThread,nullptr,testfunc_thread_starter, &threadArg)) {
        // FIXME: Some error here would probably be nice
        exit(1);
    }
    pthread_join(hThread, nullptr);
#endif
    return threadArg.returnValue;
}

#endif  // TRUN_HAVE_THREADS
