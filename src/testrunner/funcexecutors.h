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

        virtual int Execute(TestFunc *testFunc, TRUN_PRE_POST_HOOK_DELEGATE cbPreHook, TRUN_PRE_POST_HOOK_DELEGATE cbPostHook) { return -1; }
    };

    class TestFuncExecutorSequential : public TestFuncExecutorBase {
    public:
        TestFuncExecutorSequential() = default;
        virtual ~TestFuncExecutorSequential() = default;

        int Execute(TestFunc *testFunc, TRUN_PRE_POST_HOOK_DELEGATE cbPreHook, TRUN_PRE_POST_HOOK_DELEGATE cbPostHook) override;
    };

    // Only available if compiled with thread support
#ifdef TRUN_HAVE_THREADS
    class TestFuncExecutorParallel : public TestFuncExecutorSequential {
    public:
        TestFuncExecutorParallel() = default;
        virtual ~TestFuncExecutorParallel() = default;

        int Execute(TestFunc *testFunc, TRUN_PRE_POST_HOOK_DELEGATE cbPreHook, TRUN_PRE_POST_HOOK_DELEGATE cbPostHook) override;
    };

    class TestFuncExecutorParallelPThread : public TestFuncExecutorSequential {
    public:
        TestFuncExecutorParallelPThread() = default;
        virtual ~TestFuncExecutorParallelPThread() = default;

        int Execute(TestFunc *testFunc, TRUN_PRE_POST_HOOK_DELEGATE cbPreHook, TRUN_PRE_POST_HOOK_DELEGATE cbPostHook) override;

        int ThreadFunc(TestFunc *testFunc, TRUN_PRE_POST_HOOK_DELEGATE cbPreHook, TRUN_PRE_POST_HOOK_DELEGATE cbPostHook);
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
