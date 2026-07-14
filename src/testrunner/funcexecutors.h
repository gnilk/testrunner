//
// Created by gnilk on 02.05.24.
//

#ifndef TESTRUNNER_FUNC_EXECUTORS_H
#define TESTRUNNER_FUNC_EXECUTORS_H

#include "../shared/dynlib.h"
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

    // The single threaded executor: each test case runs in its own thread for isolation
    // and mid-body termination. A failing/aborting test unwinds via a thrown
    // TestAbortException, caught in TestFuncExecutorSequential::Execute.
    class TestFuncExecutorThreaded : public TestFuncExecutorSequential {
    public:
        TestFuncExecutorThreaded() = default;
        virtual ~TestFuncExecutorThreaded() = default;

        int Execute(TestFunc *testFunc, const CBPrePostHook &cbPreHook, const CBPrePostHook &cbPostHook) override;
    };

    class TestFuncExecutorFactory {
    public:
        static TestFuncExecutorBase &Create(IDynLibrary::Ref library);
    };

}

#endif //TESTRUNNER_FUNCEXECUTORS_H
