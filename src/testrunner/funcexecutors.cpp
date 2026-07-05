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
#include "responseproxy.h"
#include "testinterface_internal.h"
#include <filesystem>
#include <iostream>
#ifdef TRUN_HAVE_EXCEPTIONS
#include <cpptrace/from_current.hpp>
#endif

#include <thread>


using namespace trun;

//
// Factory function for the executor
// Class only for encapsulation...
//
TestFuncExecutorBase &TestFuncExecutorFactory::Create(IDynLibrary::Ref library) {
    // Test cases always run in their own thread. kThreaded (V2 default) and kThreadedWithExit
    // (V1 / --allow-thread-exit) share this one executor; that distinction is read only by
    // TerminateThreadIfNeeded, to decide whether Error/Assert force-terminate (Abort/Fatal
    // always do) - not here. There is no case-sequential mode: --sequential is module-scope
    // (one process, no fork), the case body still runs threaded.
    //
    // NOTE: TestFuncExecutorSequential is NOT dead - it is the shared case-running body
    // (pre-hook, invoke, exception handling, post-hook) that TestFuncExecutorThreaded calls
    // from inside its worker thread.
    static TestFuncExecutorThreaded threadedExecutor;
    threadedExecutor.SetLibrary(library);
    return threadedExecutor;
}

//
// Invoke a hook with versioning...
//
int TestFuncExecutorBase::InvokeHook(const CBPrePostHook &hook) {
    int returnCode = kTR_Pass;

    // Fetch version..
    auto rawVersion = library->GetRawVersion();
#ifdef TRUN_HAVE_EXCEPTIONS
    // Hook signatures have changed so we need to do this - a bit convoluted..
    try {
        if (rawVersion == TRUN_MAGICAL_IF_VERSION1) {
            hook.cbHookV1((ITestingV1 *) TestResponseProxy::GetTRTestInterface(library->GetVersion()));
        } else {
            returnCode = hook.cbHookV2((ITestingV2 *) TestResponseProxy::GetTRTestInterface(library->GetVersion()));
        }
    } catch(...) {
        printf(" ** Exception during pre/post hook call **\n");
        returnCode = kTR_Fail;
    }
#else
    // Sorry, no protection in this path...
    if (rawVersion == TRUN_MAGICAL_IF_VERSION1) {
            hook.cbHookV1((ITestingV1 *) TestResponseProxy::GetTRTestInterface(library->GetVersion()));
    } else {
        returnCode = hook.cbHookV2((ITestingV2 *) TestResponseProxy::GetTRTestInterface(library->GetVersion()));
    }
#endif

    return returnCode;
}
#ifdef TRUN_HAVE_EXCEPTIONS
namespace cpptrace {
    void print_frame(
            std::ostream& stream,
            bool color,
            unsigned frame_number_width,
            std::size_t counter,
            const stacktrace_frame& frame
    );
}

static std::string UnwindException(const char *exceptionString, const cpptrace::stacktrace &exception_stacktrace) {
    int idxTargetFrame = 0;
    for(const auto& frame : exception_stacktrace.frames) {

        // NOTE: we could do more here - for instance, we can insert a 'cut-line' in case the user wants an extended stack frame
        //       also we can filter the libstdc++ stuff as well - as that is not so interesting...

        // check if we have unwind to the 'testrunner' in that case we are outside the scope of the user, thus we stop
        if (frame.symbol.find("trun::") != std::string::npos) {
            idxTargetFrame -= 1;
            break;
        }
        cpptrace::print_frame(std::cout, true, 2, idxTargetFrame, frame);
        std::cout << "\n";
        idxTargetFrame++;
    }

    if (idxTargetFrame < 0) {
        return exceptionString;
    }

    auto &frame = exception_stacktrace.frames.at(idxTargetFrame);
    if (!frame.line.has_value()) {
        printf("*** EXCEPTION ERROR: %s\t'%s'\n",
               frame.filename.c_str(), exceptionString);
    } else {
        printf("*** EXCEPTION ERROR: %s:%d\t'%s'\n",
               frame.filename.c_str(), frame.line.value(), exceptionString);
    }

    return exceptionString;
}
#endif

//
// Sequential execution
//
int TestFuncExecutorSequential::Execute(TestFunc *testFunc, const CBPrePostHook &cbPreHook, const CBPrePostHook &cbPostHook) {
    // Begin test, note: THIS MUST BE DONE HERE - in case of threading!
    gnilk::ILogger  *pLogger = gnilk::Logger::GetLogger("TestFuncExeSeq");
    auto currentModule = TestRunner::GetCurrentTestModule();
    if (currentModule == nullptr) {
        pLogger->Error("No module, can't execute!");
        return -1;
    }

    auto &proxy = currentModule->GetTestResponseProxy();

    // Test-case pre function, note: cbPreHook is a union of funcptrs - doesn't matter which one I check against!
    if (cbPreHook.cbHookV1 != nullptr) {
        // Note: in case of threaded test execution, we this will terminate on error - we need to disable thread here OR we need to actually thread pre/post as well
        testFunc->ChangeExecState(TestFunc::kExecState::PreCallback);

        int preHookReturnCode = InvokeHook(cbPreHook);
        //preHookReturnCode = cbPreHook();
        if (preHookReturnCode != kTR_Pass) {
            //testReturnCode = preHookReturnCode;
            return preHookReturnCode;
        }
    }

    // Main (the actual test case)
    int testReturnCode = {};
    testFunc->ChangeExecState(TestFunc::kExecState::Main);

    // FIXME: This is not the proper way - replace!!

    // Trying 'cpptrace' to get a nice stack frame from an exception...
    // CPPTrace requires 'special' try/catch - there is some magic here
#ifdef TRUN_HAVE_EXCEPTIONS
    try {
        CPPTRACE_TRY {
            testReturnCode = testFunc->InvokeTestCase(proxy);
        } CPPTRACE_CATCH(...) {
            throw;
        }
    } catch (const TestAbortException &abort_exception) {
        // Internal forced-termination unwind used to implement Fatal/Abort and V1 asserts.
        // The proxy already holds the intended severity (ModuleFail/AllFail/TestFail); flag
        // it as a forced termination so the result is NOT downgraded to a plain TestFail the
        // way an escaped user exception is.
        proxy.SetForciblyTerminated(abort_exception.reason);
    } catch (const std::exception &e) {
        auto exception_stacktrace = cpptrace::from_current_exception();
        auto exceptionString = UnwindException(e.what(), exception_stacktrace);
        proxy.SetExceptionError(exceptionString);
        testReturnCode = kTR_Fail;
    } catch (...) {
        // unknown exception (still real failure)
        auto exception_stacktrace = cpptrace::from_current_exception();
        auto exceptionString = UnwindException("unknown exception", exception_stacktrace);
        proxy.SetExceptionError(exceptionString);
        testReturnCode = kTR_Fail;
    }

#else
    testReturnCode = testFunc->InvokeTestCase(proxy);
#endif
    // Test-case post function, note cbPostHook is a union of funcptrs - doesn't matter which one we check
    if (cbPostHook.cbHookV1 != nullptr) {
        testFunc->ChangeExecState(TestFunc::kExecState::PostCallback);
        int postHookReturnCode = InvokeHook(cbPostHook);
        //int postHookReturnCode = cbPostHook((ITestingV2 *)TestResponseProxy::GetTRTestInterface());
        if (postHookReturnCode != kTR_Pass) {
            return postHookReturnCode;
        }
    }

    // IF the thread is terminated - this won't matter - we will be in the correct state
    // However, IF post-hook is successful - but main function failed - we have changed the state and should change it back..
    if (testReturnCode != kTR_Pass) {
        testFunc->ChangeExecState(TestFunc::kExecState::Main);
    }

    return testReturnCode;
}

//
// Threaded execution.
//
// Each test case runs in its own std::thread for isolation. Forced mid-body termination
// (V1 asserts, Fatal/Abort, --allow-thread-exit) is realised by throwing TestAbortException
// from inside the test, which unwinds the thread and is caught in
// TestFuncExecutorSequential::Execute. (Without exceptions the fallback in
// TestResponseProxy::TerminateThreadIfNeeded uses pthread_exit - the no-exceptions / no-thread
// case is a swapped-in implementation, not a compile-time conditional here.)
//
struct ThreadArg {
    TestFunc *testFunc;
    TestModule::Ref testModule;
    TestRunner *testRunner;
    const CBPrePostHook &cbPreHook;
    const CBPrePostHook &cbPostHook;
    int returnValue;
};

int TestFuncExecutorThreaded::Execute(TestFunc *testFunc, const CBPrePostHook &cbPreHook, const CBPrePostHook &cbPostHook) {

    auto threadArg = ThreadArg {
            .testFunc = testFunc,
            .testModule = TestRunner::GetCurrentTestModule(),
            .testRunner = TestRunner::GetCurrentRunner(),
            .cbPreHook = cbPreHook,
            .cbPostHook = cbPostHook,
            .returnValue = -1,
    };

    auto thread = std::thread([this, &threadArg]() {

        TestRunner::SetCurrentTestModule(threadArg.testModule);
        TestRunner::SetCurrentTestRunner(threadArg.testRunner);

        threadArg.returnValue = TestFuncExecutorSequential::Execute(threadArg.testFunc, threadArg.cbPreHook, threadArg.cbPostHook);
    });

    thread.join();
    return threadArg.returnValue;
}
