#pragma once
#include "platform.h"


#include <string>
#include "testinterface.h"
#include "timer.h"
#include "logger.h"
#include "testresult.h"
#include "testhooks.h"
#include "asserterror.h"

namespace trun {

    class TestResponseProxy {
    public:
        static TestResponseProxy &Instance();

        ITesting *Proxy() { return trp; }

        void Begin(std::string symbolName, std::string moduleName);
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

        void SetPreCaseCallback(TRUN_PRE_POST_HOOK_DELEGATE cbPreCase);
        void SetPostCaseCallback(TRUN_PRE_POST_HOOK_DELEGATE cbPostCase);

        void CaseDepends(const char *caseName, const char *dependencyList);

    private:
        TestResponseProxy();
    private:
        Timer timer;
        double tElapsed;

        class AssertError assertError;

        kTestResult testResult;
        int assertCount;
        int errorCount;

        std::string symbolName; // current symbol under test
        std::string moduleName; // current library under test
        ILogger *pLogger;


        ITesting *trp;
    };
}