//
// Created by gnilk on 02.05.24.
//

#ifndef TESTRUNNER_MODULE_EXECUTORS_H
#define TESTRUNNER_MODULE_EXECUTORS_H

#include "dynlib.h"
#include "testmodule.h"

namespace trun {
    // Base class
    class TestModuleExecutorBase {
    public:
        TestModuleExecutorBase() = default;
        virtual ~TestModuleExecutorBase() = default;

        virtual bool Execute(const IDynLibrary::Ref &library, const std::map<std::string, TestModule::Ref> &testModules) { return false; };
    };

    // Sequential execution - no threads
    class TestModuleExecutorSequential : public TestModuleExecutorBase {
    public:
        TestModuleExecutorSequential() = default;
        virtual ~TestModuleExecutorSequential() = default;

        bool Execute(const IDynLibrary::Ref &library, const std::map<std::string, TestModule::Ref> &testModules) override;
    private:
        gnilk::ILogger *pLogger = nullptr;
    };

    // parallel execution - each module in a separate thread
    class TestModuleExecutorParallel : public TestModuleExecutorBase {
    public:
        TestModuleExecutorParallel() = default;
        virtual ~TestModuleExecutorParallel() = default;

        bool Execute(const IDynLibrary::Ref &library, const std::map<std::string, TestModule::Ref> &testModules) override;
    private:
        gnilk::ILogger *pLogger = nullptr;
    };

    // A bit of enterprise perhaps?  - in this case it sort of makes sense...
    class TestModuleExecutorFactory {
    public:
        static TestModuleExecutorBase &Create();
    };

}
#endif //TESTRUNNER_MODULE_EXECUTORS_H
