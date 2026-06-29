//
// Created by gnilk on 02.05.24.
//

#ifndef TESTRUNNER_MODULE_EXECUTORS_H
#define TESTRUNNER_MODULE_EXECUTORS_H

#include "../shared/dynlib.h"
#include "testmodule.h"

namespace trun {
    // Base class
    class TestModuleExecutorBase {
    public:
        TestModuleExecutorBase() = default;
        virtual ~TestModuleExecutorBase() = default;

        virtual bool Execute(const IDynLibrary::Ref &library, const std::map<std::string, TestModule::Ref> &testModules) { return false; };
    protected:
        gnilk::ILogger *pLogger = nullptr;
    };

    // Sequential execution - no threads
    class TestModuleExecutorSequential : public TestModuleExecutorBase {
    public:
        TestModuleExecutorSequential() = default;
        virtual ~TestModuleExecutorSequential() = default;

        bool Execute(const IDynLibrary::Ref &library, const std::map<std::string, TestModule::Ref> &testModules) override;
    };

    // NOTE: a thread-per-module executor used to live here (TestModuleExecutorParallel).
    // It was unreachable in every build: on desktop TRUN_HAVE_FORK is always set, so the
    // factory maps kParallel -> fork; without fork the module loop runs sequentially.
    // The fork executor superseded it. Removed deliberately.

#ifdef TRUN_HAVE_FORK
    class TestModuleExecutorFork : public TestModuleExecutorBase {
    public:
        TestModuleExecutorFork() = default;
        virtual ~TestModuleExecutorFork() = default;

        bool Execute(const IDynLibrary::Ref &library, const std::map<std::string, TestModule::Ref> &testModules) override;
    };
#endif

    // A bit of enterprise perhaps?  - in this case it sort of makes sense...
    class TestModuleExecutorFactory {
    public:
        static TestModuleExecutorBase &Create();
    };

}
#endif //TESTRUNNER_MODULE_EXECUTORS_H
