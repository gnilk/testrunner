//
// Created by gnilk on 02.05.24.
//

#ifndef TESTRUNNER_FUNC_EXECUTORS_H
#define TESTRUNNER_FUNC_EXECUTORS_H

#include "dynlib.h"
#include "testhooks.h"
#include "testfunc.h"

namespace trun {

    class TestFuncExecutorBase {
    public:
        TestFuncExecutorBase() = default;
        virtual ~TestFuncExecutorBase() = default;

        virtual int Execute(TestFunc *testFunc, TRUN_PRE_POST_HOOK_DELEGATE_V2 cbPreHook, TRUN_PRE_POST_HOOK_DELEGATE_V2 cbPostHook) { return -1; }
    protected:
        int InvokeHook(TRUN_PRE_POST_HOOK_DELEGATE_V2 cbHook);
    };

    class TestFuncExecutorSequential : public TestFuncExecutorBase {
    public:
        TestFuncExecutorSequential() = default;
        virtual ~TestFuncExecutorSequential() = default;

        int Execute(TestFunc *testFunc, TRUN_PRE_POST_HOOK_DELEGATE_V2 cbPreHook, TRUN_PRE_POST_HOOK_DELEGATE_V2 cbPostHook) override;
    };

    // Only available if compiled with thread support
#ifdef TRUN_HAVE_THREADS
    class TestFuncExecutorParallel : public TestFuncExecutorSequential {
    public:
        TestFuncExecutorParallel() = default;
        virtual ~TestFuncExecutorParallel() = default;

        int Execute(TestFunc *testFunc, TRUN_PRE_POST_HOOK_DELEGATE_V2 cbPreHook, TRUN_PRE_POST_HOOK_DELEGATE_V2 cbPostHook) override;
    };

    class TestFuncExecutorParallelPThread : public TestFuncExecutorSequential {
    public:
        TestFuncExecutorParallelPThread() = default;
        virtual ~TestFuncExecutorParallelPThread() = default;

        int Execute(TestFunc *testFunc, TRUN_PRE_POST_HOOK_DELEGATE_V2 cbPreHook, TRUN_PRE_POST_HOOK_DELEGATE_V2 cbPostHook) override;

        int ThreadFunc(TestFunc *testFunc, TRUN_PRE_POST_HOOK_DELEGATE_V2 cbPreHook, TRUN_PRE_POST_HOOK_DELEGATE_V2 cbPostHook);
    protected:
        int returnValue = {};
    };
#endif

    class TestFuncExecutorFactory {
    public:
        static TestFuncExecutorBase &Create();
    };

}

#endif //TESTRUNNER_FUNCEXECUTORS_H
