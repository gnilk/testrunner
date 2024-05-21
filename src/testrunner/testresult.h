#pragma once
/*-------------------------------------------------------------------------
 File    : testresult.h
 Author  : FKling
 Version : -
 Orginal : 2018-10-18
 Descr   : Container for test results


 Part of testrunner
 BSD3 License!
 
 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>

 </pre>
 
 
 \History
 - 2018.10.18, FKling, Implementation
 
 ---------------------------------------------------------------------------*/

#include "platform.h"
#include <string>
#include <memory>
#include "asserterror.h"

namespace trun {

    typedef enum : uint8_t {
        kTestResult_Pass = 0,
        kTestResult_TestFail = 1,
        kTestResult_ModuleFail = 2,
        kTestResult_AllFail = 3,
        kTestResult_NotExecuted = 4,
        kTestResult_InvalidReturnCode = 5,
    } kTestResult;

    class TestResult {
    public:
        using Ref = std::shared_ptr<TestResult>;
        enum class kFailState {
            None,
            PreHook,
            Main,
            PostHook,
        };
        enum class kRunResultAction {
            kContinue,
            kAbortModule,
            kAbortAll,
        };

    public:
        static TestResult::Ref Create(const std::string &symbolName);
        TestResult(const std::string &symbolName);

        // Returns the next action for the runner depending on the result..
        TestResult::kRunResultAction CheckIfContinue() const;

        // Property access, getters
        kTestResult Result() const { return testResult; }
        int Errors() const { return numError; }
        int Asserts() const { return numAssert; }
        double ElapsedTimeSec() const { return tElapsedSec; }
        const std::string &SymbolName() const { return symbolName; }
        kFailState FailState() { return failState; }
        const std::string &FailStateName() {
            if (failState == kFailState::PreHook) {
                static const std::string strPreHook = "pre-hook";
                return strPreHook;
            }
            // Just call if outside of main
            static const std::string strPostHook = "post-hook";
            return strPostHook;
        }

        const class AssertError &AssertError() const { return assertError; };

        void SetTestResultFromReturnCode(int testReturnCode);

        // Setters
        void SetFailState(kFailState newFailState) { failState = newFailState; }
        void SetResult(kTestResult newResult) { testResult = newResult; }
        void SetTimeElapsedSec(double t) { tElapsedSec = t; }
        void SetNumberOfErrors(int count) { numError = count; }
        void SetNumberOfAsserts(int count) { numAssert = count; }
        void SetAssertError(class AssertError &other);
    private:
        class AssertError assertError;
        kFailState failState = kFailState::None;
        kTestResult testResult = kTestResult_NotExecuted;
        double tElapsedSec = 0;
        int numError = 0;
        int numAssert = 0;
        std::string symbolName;
    };
}
