/*-------------------------------------------------------------------------
 File    : responseproxy.cpp
 Author  : FKling
 Version : -
 Orginal : 2018-10-18
 Descr   : implements C/C++ calling convention wrappers for test reponses

 Proxy between flat-c callback functions and test execution handling.
 Only one proxy per thread. Test must be executed in that thread as well (for now)

 Part of testrunner
 BSD3 License!
 
 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TO-DO: [ -:Not done, +:In progress, !:Completed]
 <pre>

 </pre>
 
 
 \History
 - 2018.10.18, FKling, Implementation
 
 ---------------------------------------------------------------------------*/
#ifdef WIN32
#include <Windows.h>
#else
#include <thread>
#include <pthread.h>
#include <stdarg.h>
#endif

#include "testinterface.h"
#include "responseproxy.h"
#include "logger.h"
#include "config.h"
#include "testrunner.h"


#include <stdlib.h> // malloc

#include <map>
#include <string>

using namespace trun;

// Trampoline functions
static void int_trp_debug(int line, const char *file, const char *format, ...);
static void int_trp_info(int line, const char *file, const char *format, ...);
static void int_trp_warning(int line, const char *file, const char *format, ...);
static void int_trp_error(int line, const char *file, const char *format, ...);
static void int_trp_fatal(int line, const char *file, const char *format, ...);
static void int_trp_abort(int line, const char *file, const char *format, ...);
static void int_trp_assert_error(const char *exp, const char *file, int line);
static void int_trp_hook_precase(TRUN_PRE_POST_HOOK_DELEGATE cbPreCase);
static void int_trp_hook_postcase(TRUN_PRE_POST_HOOK_DELEGATE cbPostCase);
static void int_trp_casedepend(const char *caseName, const char *dependencyList);

// Holds a calling proxy per thread
#ifdef WIN32
static std::map<DWORD, TestResponseProxy*> trpLookup;
#else
static std::map<pthread_t, TestResponseProxy *> trpLookup;
#endif

//
// GetInstance - returns a test proxy, if one does not exists for the thread one will be allocated
//
TestResponseProxy &TestResponseProxy::Instance() {
    // NOTE: Figure out how this should work
    //       Where is threading occuring and who owns this
    //       Currently this function is called from the 'testfunc.cpp' where also threading
    //       is happening. But that's not quite right. Instead a library should have this in order
    //       to allow parallell testing of modules but not within modules!
    //
    //       [gnilk, 2024-02-27] - Each TestFunc could have an instance of the TestResponseProxy as it is designed today
    //                             That would allow all (except dependencies) to be executed in parallell..
    //                             However - that would probably blow up memusage on embedded quite a bit..
    //

    static TestResponseProxy glbResponseProxy;
    return glbResponseProxy;
}


//
TestResponseProxy::TestResponseProxy() {
    // FIXME: Refactor this once we know how the TestResponseProxy will fit in the whole testing library
    this->trp = (ITesting *)malloc(sizeof(ITesting));
    this->trp->Debug = int_trp_debug;
    this->trp->Info = int_trp_info;
    this->trp->Warning = int_trp_warning;
    this->trp->Error = int_trp_error;
    this->trp->Fatal = int_trp_fatal;
    this->trp->Abort = int_trp_abort;
    this->trp->AssertError = int_trp_assert_error;
    this->trp->SetPreCaseCallback = int_trp_hook_precase;
    this->trp->SetPostCaseCallback = int_trp_hook_postcase;
    this->trp->CaseDepends = int_trp_casedepend;
}

void TestResponseProxy::Begin(std::string symbolName, std::string moduleName) {
    this->symbolName = symbolName;
    this->moduleName = moduleName;
    errorCount = 0;
    assertCount = 0;
    testResult = kTestResult_Pass;
    pLogger = Logger::GetLogger(moduleName.c_str());

    // Apply verbose filtering to log output from test cases or not??
    if (!Config::Instance().testLogFilter) {
        pLogger->Enable(Logger::kFlags_PassThrough);
    } else {
        pLogger->Enable(Logger::kFlags_BlockAll);
    } 
    

    timer.Reset();
    assertError.Reset();
}

double TestResponseProxy::ElapsedTimeInSec() {
    return tElapsed;
}

void TestResponseProxy::End() {
    tElapsed = timer.Sample();
    symbolName.clear();    
    moduleName.clear();
    pLogger = nullptr;
}

int TestResponseProxy::Errors() {
    return errorCount;
}

int TestResponseProxy::Asserts() {
    return assertCount;
}

kTestResult TestResponseProxy::Result() {
    return testResult;
}

// ITesting mirror
void TestResponseProxy::Debug(int line, const char *file, std::string message) {
    pLogger->Debug("%s:%d:%s", file, line, message.c_str());
}
void TestResponseProxy::Info(int line, const char *file, std::string message) {
    pLogger->Info("%s:%d:%s", file, line, message.c_str());
}
void TestResponseProxy::Warning(int line, const char *file, std::string message) {
    pLogger->Warning("%s:%d:%s", file, line, message.c_str());
}

//
// All error functions will abort the running test!!!
//
void TestResponseProxy::Error(int line, const char *file, std::string message) {
    pLogger->Error("%s:%d:%s", file, line, message.c_str());
    this->errorCount++;
    if (testResult < kTestResult_TestFail) {
        testResult = kTestResult_TestFail;
    }
#ifdef TRUN_HAVE_THREADS
    #ifdef WIN32
        TerminateThread(GetCurrentThread(),0);
    #else
        pthread_exit(NULL);
    #endif
#endif
}

void TestResponseProxy::Fatal(int line, const char *file, std::string message) {
    pLogger->Critical("%s:%d: %s", file, line, message.c_str());
    this->errorCount++;
    if (testResult < kTestResult_ModuleFail) {
        testResult = kTestResult_ModuleFail;
    }
    assertError.Set(AssertError::kAssert_Fatal, line, file, message);

#ifdef TRUN_HAVE_THREADS
    #ifdef WIN32
        TerminateThread(GetCurrentThread(), 0);
    #else
        pthread_exit(NULL);
    #endif
#endif
}

void TestResponseProxy::Abort(int line, const char *file, std::string message) {
    pLogger->Fatal("%s:%d: %s", file, line, message.c_str());
    this->errorCount++;
    if (testResult < kTestResult_AllFail) {
        testResult = kTestResult_AllFail;
    }
    assertError.Set(AssertError::kAssert_Abort, line, file, message);
#ifdef TRUN_HAVE_THREADS
    #ifdef WIN32
        if (!TerminateThread(GetCurrentThread(), 0)) {
            pLogger->Error("Terminating thread...\n");
        }
    #else
        pthread_exit(NULL);
    #endif
#endif
}

//void (*AssertError)(const char *exp, const char *file, const int line);
void TestResponseProxy::AssertError(const char *exp, const char *file, const int line) {
    pLogger->Error("Assert Error: %s:%d\t'%s'", file, line, exp);
    this->assertCount++;
    if (testResult < kTestResult_TestFail) {
        testResult = kTestResult_TestFail;
    }
    assertError.Set(AssertError::kAssert_Error, line, file, exp);
#ifdef TRUN_HAVE_THREADS
    #ifdef WIN32
        TerminateThread(GetCurrentThread(), 0);
    #else
        pthread_exit(NULL);
    #endif
#endif
}


void TestResponseProxy::SetPreCaseCallback(TRUN_PRE_POST_HOOK_DELEGATE cbPreCase) {
    auto testModule = TestRunner::HACK_GetCurrentTestModule();
    if (testModule != nullptr) {
        testModule->cbPreHook = cbPreCase;
    }
}

void TestResponseProxy::SetPostCaseCallback(TRUN_PRE_POST_HOOK_DELEGATE cbPostCase) {
    auto testModule = TestRunner::HACK_GetCurrentTestModule();
    if (testModule != nullptr) {
        testModule->cbPostHook = cbPostCase;
    }
}

void TestResponseProxy::CaseDepends(const char *caseName, const char *dependencyList) {
    auto testModule = TestRunner::HACK_GetCurrentTestModule();
    if (testModule != nullptr) {
        testModule->SetDependencyForCase(caseName, dependencyList);
    }
}


//
// wrappers for pure C call's (no this) - only one call per thread allowed.
//

#if defined(TRUN_EMBEDDED_MCU)
    #define CREATE_REPORT_STRING() \
    const char *newstr="";               \

#else

#define CREATE_REPORT_STRING() \
	va_list	values;													\
	char * newstr = NULL;											\
    uint32_t szbuf = 1024;                                          \
    int res;                                                        \
    do																\
    {																\
        newstr = (char *)alloca(szbuf);                             \
        va_start( values, format );   								\
        res = vsnprintf(newstr, szbuf, format, values);	            \
        va_end(	values);											\
        if (res < 0) {												\
            szbuf += 1024;											\
        }															\
        if (!IsMsgSizeOk(szbuf)) {                                  \
            break;                                                  \
        }                                                           \
    } while(res < 0);												\

#endif

static bool IsMsgSizeOk(uint32_t szbuf) {
    if (szbuf > Config::Instance().responseMsgByteLimit) {
        auto pLogger = Logger::GetLogger("TestResponseProxy");
        pLogger->Error("Message buffer exceeds limit (%d bytes), truncating..");
        return false;
    }
    return true;
}

static void int_trp_debug(int line, const char *file, const char *format, ...) {
    CREATE_REPORT_STRING()
    TestResponseProxy::Instance().Debug(line, file, std::string(newstr));
}
static void int_trp_info(int line, const char *file, const char *format, ...) {
    CREATE_REPORT_STRING()
    TestResponseProxy::Instance().Info(line, file, std::string(newstr));
}
static void int_trp_warning(int line, const char *file, const char *format, ...) {
    CREATE_REPORT_STRING()
    TestResponseProxy::Instance().Warning(line, file, std::string(newstr));
}
static void int_trp_error(int line, const char *file, const char *format, ...) {
    CREATE_REPORT_STRING()
    TestResponseProxy::Instance().Error(line, file, std::string(newstr));
}

static void int_trp_fatal(int line, const char *file, const char *format, ...) {
    CREATE_REPORT_STRING()
    TestResponseProxy::Instance().Fatal(line, file, std::string(newstr));
}

static void int_trp_abort(int line, const char *file, const char *format, ...) {
    CREATE_REPORT_STRING()
    TestResponseProxy::Instance().Abort(line,file, std::string(newstr));
}

static void int_trp_assert_error(const char *exp, const char *file, int line) {
    TestResponseProxy::Instance().AssertError(exp, file, line);
}

#undef CREATE_REPORT_STRING

static void int_trp_hook_precase(TRUN_PRE_POST_HOOK_DELEGATE cbPreCase) {
    TestResponseProxy::Instance().SetPreCaseCallback(cbPreCase);
}

static void int_trp_hook_postcase(TRUN_PRE_POST_HOOK_DELEGATE cbPostCase) {
    TestResponseProxy::Instance().SetPostCaseCallback(cbPostCase);
}

static void int_trp_casedepend(const char *caseName, const char *dependencyList) {
    TestResponseProxy::Instance().CaseDepends(caseName, dependencyList);
}

