//
// Created by gnilk on 02.05.24.
//

#ifndef TESTRUNNER_FUNC_EXECUTORS_H
#define TESTRUNNER_FUNC_EXECUTORS_H

#include "dynlib.h"
#include "testfunc.h"

namespace trun {

    class TestFuncExecutorBase {
    public:
        TestFuncExecutorBase() = default;
        virtual ~TestFuncExecutorBase() = default;

        void SetLibrary(IDynLibrary::Ref useLibrary) { library = useLibrary; }
        IDynLibrary::Ref GetLibrary() { return library; }
        virtual int Execute(TestFunc *testFunc, const CBPrePostHook &cbPreHook, const CBPrePostHook &cbPostHook) { return -1; }
    protected:
        int InvokeHook(const CBPrePostHook &cbHook);
        IDynLibrary::Ref library = nullptr;
    };

    class TestFuncExecutorSequential : public TestFuncExecutorBase {
    public:
        TestFuncExecutorSequential() = default;
        virtual ~TestFuncExecutorSequential() = default;

        int Execute(TestFunc *testFunc, const CBPrePostHook &cbPreHook, const CBPrePostHook &cbPostHook) override;
    };

    // Only available if compiled with thread support
#ifdef TRUN_HAVE_THREADS
    class TestFuncExecutorParallel : public TestFuncExecutorSequential {
    public:
        TestFuncExecutorParallel() = default;
        virtual ~TestFuncExecutorParallel() = default;

        int Execute(TestFunc *testFunc, const CBPrePostHook &cbPreHook, const CBPrePostHook &cbPostHook) override;
    };

    class TestFuncExecutorParallelPThread : public TestFuncExecutorSequential {
    public:
        TestFuncExecutorParallelPThread() = default;
        virtual ~TestFuncExecutorParallelPThread() = default;

        int Execute(TestFunc *testFunc, const CBPrePostHook &cbPreHook, const CBPrePostHook &cbPostHook) override;

        int ThreadFunc(TestFunc *testFunc, const CBPrePostHook &cbPreHook, const CBPrePostHook &cbPostHook);
    protected:
        int returnValue = {};
    };
#endif

    class TestFuncExecutorFactory {
    public:
        static TestFuncExecutorBase &Create(IDynLibrary::Ref library);
    };

}

#endif //TESTRUNNER_FUNCEXECUTORS_H
