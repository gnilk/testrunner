#pragma once
#include "platform.h"


#include <string>
#include "../shared/testlibversion.h"
#include "testinterface_internal.h"
#include "../shared/timer.h"
#include "logger.h"
#include "testresult.h"
#include "asserterror.h"

namespace trun {

    struct TestAbortException final {
        const char *reason = nullptr;

        explicit TestAbortException(const char *r = nullptr) noexcept
            : reason(r) {}
    };

    // FIXME: Support for exceptions
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
        bool IsAssertValid() const { return assertError.IsValid(); }
        class AssertError &GetAssertError() { return assertError; }

        void SetExceptionError(const std::string &exception);
        bool WasExceptionThrown() const {
            return exceptionThrown;
        }
        const std::string &GetExceptionString() const {
            return exceptionString;
        }
    public: // ITesting mirroring
        void Debug(int line, const char *file, std::string message);
        void Info(int line, const char *file, std::string message);
        void Warning(int line, const char *file, std::string message);
        void Error(int line, const char *file, std::string message);
        void Fatal(int line, const char *file, std::string message);
        void Abort(int line, const char *file, std::string message);

        kTRContinueMode AssertError(const char *exp, const char *file, const int line);

        void SetPreCaseCallback(const CBPrePostHook &cbPreCase);
        void SetPostCaseCallback(const CBPrePostHook &cbPostCase);

        void CaseDepends(const char *caseName, const char *dependencyList);
        void ModuleDepends(const char *moduleName, const char *dependencyList);

        void QueryInterface(uint32_t interface_id, void **outPtr);

        // Returns
        //  ITestingVn  - where V depends on version which is resolved by looking for the TRUN_MAGICAL_IF_VERSION in the shared library...
        static ITestingVersioned *GetTRTestInterface(const Version &version);
    private:
        static ITestingConfig *GetTRConfigInterface();
        static ITestingCoverage *GetTRCoverageInterface();
        void TerminateThreadIfNeeded(bool alwaysTerminate);

        // Helpers to fetch interface of the correct version
        static ITestingV1 *GetTRTestInterfaceV1();
        static ITestingV2 *GetTRTestInterfaceV2();

    private:
        // NOTE: All of these must be cleared during 'begin' - this is a terrible design decision...
        Timer timer;
        double tElapsed;

        class AssertError assertError;
        bool exceptionThrown = false;
        std::string exceptionString = {};
        kTestResult testResult = {};
        int assertCount = 0;
        int errorCount = 0;

        std::string symbolName; // current symbol under test
        std::string moduleName; // current library under test

        gnilk::ILogger *pLogger = nullptr;
    };
}