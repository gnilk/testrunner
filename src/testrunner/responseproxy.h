#pragma once
#include "platform.h"


#include <string>
#include "testinterface_internal.h"
#include "timer.h"
#include "logger.h"
#include "testresult.h"
#include "testhooks.h"
#include "asserterror.h"

namespace trun {

    class TestResponseProxy {
    public:
        TestResponseProxy() = default;
        virtual ~TestResponseProxy() = default;

        void Begin(const std::string &symbolName, const std::string &moduleName);
        void End();

        double ElapsedTimeInSec();
        int Errors();
        int Asserts();
        kTestResult Result();
        bool IsAssertValid() const { return assertError.isValid; }
        class AssertError &GetAssertError() { return assertError; }


    public: // ITesting mirroring
        void Debug(int line, const char *file, std::string message);
        void Info(int line, const char *file, std::string message);
        void Warning(int line, const char *file, std::string message);
        void Error(int line, const char *file, std::string message);
        void Fatal(int line, const char *file, std::string message);
        void Abort(int line, const char *file, std::string message);

        void AssertError(const char *exp, const char *file, const int line);

        void SetPreCaseCallback(const CBPrePostHook &cbPreCase);
        void SetPostCaseCallback(const CBPrePostHook &cbPostCase);

        void CaseDepends(const char *caseName, const char *dependencyList);
        void ModuleDepends(const char *moduleName, const char *dependencyList);

        void QueryInterface(uint32_t interface_id, void **outPtr);

        static void *GetTRTestInterface(uint32_t version);
    private:
        static ITestingConfig *GetTRConfigInterface();

        void TerminateThreadIfNeeded();
    private:
        Timer timer;
        double tElapsed;

        class AssertError assertError;

        kTestResult testResult;
        int assertCount;
        int errorCount;

        std::string symbolName; // current symbol under test
        std::string moduleName; // current library under test

        gnilk::ILogger *pLogger = nullptr;
    };
}