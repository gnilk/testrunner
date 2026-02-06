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
#include "ipc/IPCBufferedWriter.h"
#include "ipc/IPCEncoder.h"
#ifdef WIN32
#include <Windows.h>
#else
#include <thread>
#include <pthread.h>
#include <stdarg.h>
#include <signal.h>
#include "unix/IPCFifoUnix.h"
#endif

#include <string.h>
#include "testinterface_internal.h"
#include "responseproxy.h"
#include "logger.h"
#include "config.h"
#include "testrunner.h"
#include "CoverageIPCMessages.h"


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
static kTRContinueMode int_trp_assert_error_v2(int line, const char *file, const char *exp);
static void int_trp_hook_precase_v2(TRUN_PRE_POST_HOOK_DELEGATE_V2 cbPreCase);
static void int_trp_hook_postcase_v2(TRUN_PRE_POST_HOOK_DELEGATE_V2 cbPostCase);
static void int_trp_casedepend(const char *caseName, const char *dependencyList);
static void int_trp_moduledepend(const char *moduleName, const char *dependencyList);
static void int_trp_query_interface(uint32_t interface_id, void **ouPtr);

// V1
static void int_trp_hook_precase_v1(TRUN_PRE_POST_HOOK_DELEGATE_V1 cbPreCase);
static void int_trp_hook_postcase_v1(TRUN_PRE_POST_HOOK_DELEGATE_V1 cbPostCase);




// Config interface
static size_t int_tcfg_list(size_t maxItems, TRUN_ConfigItem *outArray);
static void int_tcfg_get(const char *key, TRUN_ConfigItem *outValue);

// Coverage
static void int_tcov_begincov(const char *symbol);

//
// Setup the test response proxy before a test is invoked
//
void TestResponseProxy::Begin(const std::string &use_symbolName, const std::string &use_moduleName) {
    symbolName = use_symbolName;
    moduleName = use_moduleName;
    errorCount = 0;
    assertCount = 0;
    testResult = kTestResult_Pass;
    pLogger = gnilk::Logger::GetLogger("TestResponseProxy");

    // Reset the timer
    timer.Reset();
    assertError.Reset();
}

//
//
//
void TestResponseProxy::End() {
    tElapsed = timer.Sample();
    pLogger = nullptr;
}


// Consider moving this out of here

ITestingVersioned *TestResponseProxy::GetTRTestInterface(const Version &version) {
    // First distinguish from major - then if needed we can check minor..
    switch(version.Major()) {
        case 1 :
            return GetTRTestInterfaceV1();
        case 2 :
            return GetTRTestInterfaceV2();
        default:
            fprintf(stderr, "Critical error, unsupported version: %s\n", version.AsString().c_str());
            exit(1);
    }
    // Never reach...
}

// Move to other file
ITestingConfig *TestResponseProxy::GetTRConfigInterface() {
    static ITestingConfig trp_config_bridge = {
            .List = int_tcfg_list,
            .Get = int_tcfg_get,
    };
    return &trp_config_bridge;
}
ITestingCoverage *TestResponseProxy::GetTRCoverageInterface() {
    static ITestingCoverage trp_coverage_bridge = {
        .BeginCoverage =  int_tcov_begincov,
    };
    return &trp_coverage_bridge;
}

double TestResponseProxy::ElapsedTimeInSec() {
    return tElapsed;
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

void TestResponseProxy::SetExceptionError(const std::string &exception) {
    exceptionThrown = true;
    exceptionString = exception;
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
// Note: These function may abort the running thread if threads are enabled (default = yes) and we are using
// pthreads (--pthreads) or on Windows..
//
void TestResponseProxy::Error(int line, const char *file, std::string message) {
    pLogger->Error("%s:%d\t'%s'", file, line, message.c_str());

    printf("*** GENERAL ERROR: %s:%d\t'%s'\n", file, line, message.c_str());
    this->errorCount++;
    if (testResult < kTestResult_TestFail) {
        testResult = kTestResult_TestFail;
    }
    TerminateThreadIfNeeded();
}

void TestResponseProxy::Fatal(int line, const char *file, std::string message) {
    pLogger->Debug("%s:%d:\t'%s'", file, line, message.c_str());

    printf("*** FATAL ERROR: %s:%d\t'%s'\n", file, line, message.c_str());
    this->errorCount++;
    if (testResult < kTestResult_ModuleFail) {
        testResult = kTestResult_ModuleFail;
    }
    assertError.Add(AssertError::kAssert_Fatal, line, file, message);
    TerminateThreadIfNeeded();
}

void TestResponseProxy::Abort(int line, const char *file, std::string message) {
    pLogger->Debug("Abort Error: %s:%d\t'%s'", file, line, message.c_str());

    printf("*** ABORT ERROR: %s:%d\t'%s'\n", file, line, message.c_str());
    this->errorCount++;
    if (testResult < kTestResult_AllFail) {
        testResult = kTestResult_AllFail;
    }
    assertError.Add(AssertError::kAssert_Abort, line, file, message);
    TerminateThreadIfNeeded();
}

kTRContinueMode TestResponseProxy::AssertError(const char *exp, const char *file, const int line) {
    pLogger->Debug("Assert Error: %s:%d\t'%s'", file, line, exp);
    printf("*** ASSERT ERROR: %s:%d\t'%s'\n", file, line, exp);

    this->assertCount++;
    if (testResult < kTestResult_TestFail) {
        testResult = kTestResult_TestFail;
    }
    assertError.Add(AssertError::kAssert_Error, line, file, exp);
    if (Config::Instance().continueOnAssert) {
        return kTRContinueMode::kTRContinue;
    }
    TerminateThreadIfNeeded();
    return kTRContinueMode::kTRLeave;
}

// Terminates the running thread if allowed - i.e. you must have 'allowThreadTermination' enabled...
void TestResponseProxy::TerminateThreadIfNeeded() {

    // FIXME: In case of V1 we should have this enabled - but we don't know the library at this point
#ifdef TRUN_HAVE_THREADS
    if (Config::Instance().testExecutionType == TestExecutiontype::kThreadedWithExit) {
        #ifdef WIN32
            TerminateThread(GetCurrentThread(), 0);
        #else
            pthread_exit(NULL);
        #endif
    }
#endif

}


void TestResponseProxy::SetPreCaseCallback(const CBPrePostHook &cbPreCase) {
    auto testModule = TestRunner::GetCurrentTestModule();
    if (testModule != nullptr) {
        testModule->cbPreHook = cbPreCase;
    }
}

void TestResponseProxy::SetPostCaseCallback(const CBPrePostHook &cbPostCase) {
    auto testModule = TestRunner::GetCurrentTestModule();
    if (testModule != nullptr) {
        testModule->cbPostHook = cbPostCase;
    }
}

void TestResponseProxy::CaseDepends(const char *caseName, const char *dependencyList) {
    auto testModule = TestRunner::GetCurrentTestModule();
    if (testModule != nullptr) {
        testModule->AddDependencyForCase(caseName, dependencyList);
    }
}

// FIXME: Implement
void TestResponseProxy::ModuleDepends(const char *modName, const char *dependencyList) {
    auto ptrRunner = TestRunner::GetCurrentRunner();


    if (ptrRunner == nullptr) {
        pLogger->Error("No test runner!");
        return;
    }
    ptrRunner->AddDependenciesForModule(modName, dependencyList);
}

void TestResponseProxy::QueryInterface(uint32_t interface_id, void **outPtr) {
    if (outPtr == nullptr) {
        pLogger->Error("QueryInterface, outPtr is null");
    }

    switch(interface_id) {
        case ITestingConfig_IFace_ID :
            pLogger->Debug("QueryInterface, interface_id = %d, returning TRUN_IConfig", interface_id);
            *outPtr = (void *)GetTRConfigInterface();
            break;
        case ITestingCoverage_IFace_ID :
            pLogger->Debug("QueryInterface, interface_id = %d, returning TRUN_ICoverage", interface_id);
            *outPtr = (void *)GetTRCoverageInterface();
            break;
        default :
            *outPtr = nullptr;
            pLogger->Debug("QueryInterface, invalid interface id (%d)", interface_id);
            break;
    }

}

// the ITestingVx inherits from empty ITestingVersioned to allow type checking on various places (alt. would be 'void *)
// however, while ITestingVersioned is an empty structure I get complaints about anonymous fields not being initialized..

#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

// static private helpers
ITestingV1 *TestResponseProxy::GetTRTestInterfaceV1() {
    static ITestingV1 trp_bridge_v1 = {
            .Debug = int_trp_debug,
            .Info = int_trp_info,
            .Warning = int_trp_warning,
            .Error = int_trp_error,
            .Fatal = int_trp_fatal,
            .Abort = int_trp_abort,
            .AssertError = int_trp_assert_error,
            .SetPreCaseCallback = int_trp_hook_precase_v1,
            .SetPostCaseCallback = int_trp_hook_postcase_v1,
            .CaseDepends = int_trp_casedepend,
    };
    return &trp_bridge_v1;
}
ITestingV2 *TestResponseProxy::GetTRTestInterfaceV2() {
    static ITestingV2 trp_bridge_v2 = {
            .Debug = int_trp_debug,
            .Info = int_trp_info,
            .Warning = int_trp_warning,
            .Error = int_trp_error,
            .Fatal = int_trp_fatal,
            .Abort = int_trp_abort,
            .AssertError = int_trp_assert_error_v2,
            .SetPreCaseCallback = int_trp_hook_precase_v2,
            .SetPostCaseCallback = int_trp_hook_postcase_v2,
            .CaseDepends = int_trp_casedepend,
            .ModuleDepends = int_trp_moduledepend,
            .QueryInterface = int_trp_query_interface,
    };
    return &trp_bridge_v2;
}
#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif

//
// Trampoline stuff below this point
//

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
        auto pLogger = gnilk::Logger::GetLogger("TestResponseProxy");
        pLogger->Error("Message buffer exceeds limit (%d bytes), truncating..");
        return false;
    }
    return true;
}

static void int_trp_debug(int line, const char *file, const char *format, ...) {
    CREATE_REPORT_STRING()
    TestRunner::GetCurrentTestModule()->GetTestResponseProxy().Debug(line, file, std::string(newstr));
}
static void int_trp_info(int line, const char *file, const char *format, ...) {
    CREATE_REPORT_STRING()
    TestRunner::GetCurrentTestModule()->GetTestResponseProxy().Info(line, file, std::string(newstr));
}
static void int_trp_warning(int line, const char *file, const char *format, ...) {
    CREATE_REPORT_STRING()
    TestRunner::GetCurrentTestModule()->GetTestResponseProxy().Warning(line, file, std::string(newstr));
}
static void int_trp_error(int line, const char *file, const char *format, ...) {
    CREATE_REPORT_STRING()
    TestRunner::GetCurrentTestModule()->GetTestResponseProxy().Error(line, file, std::string(newstr));
}
static void int_trp_fatal(int line, const char *file, const char *format, ...) {
    CREATE_REPORT_STRING()
    TestRunner::GetCurrentTestModule()->GetTestResponseProxy().Fatal(line, file, std::string(newstr));
}
static void int_trp_abort(int line, const char *file, const char *format, ...) {
    CREATE_REPORT_STRING()
    TestRunner::GetCurrentTestModule()->GetTestResponseProxy().Abort(line, file, std::string(newstr));
}
static void int_trp_assert_error(const char *exp, const char *file, int line) {
    TestRunner::GetCurrentTestModule()->GetTestResponseProxy().AssertError(exp, file, line);
}

static kTRContinueMode int_trp_assert_error_v2(int line, const char *file, const char *exp) {
    // Just swizzle the arguments back to the old interface..
    return TestRunner::GetCurrentTestModule()->GetTestResponseProxy().AssertError(exp, file, line);
}


#undef CREATE_REPORT_STRING

static void int_trp_hook_precase_v2(TRUN_PRE_POST_HOOK_DELEGATE_V2 cbPreCase) {
    CBPrePostHook hook;
    hook.cbHookV2 = cbPreCase;
    TestRunner::GetCurrentTestModule()->GetTestResponseProxy().SetPreCaseCallback(hook);
}
static void int_trp_hook_postcase_v2(TRUN_PRE_POST_HOOK_DELEGATE_V2 cbPostCase) {
    CBPrePostHook hook;
    hook.cbHookV2 = cbPostCase;
    TestRunner::GetCurrentTestModule()->GetTestResponseProxy().SetPostCaseCallback(hook);
}

static void int_trp_casedepend(const char *caseName, const char *dependencyList) {
    TestRunner::GetCurrentTestModule()->GetTestResponseProxy().CaseDepends(caseName, dependencyList);
}
static void int_trp_moduledepend(const char *moduleName, const char *dependencyList) {
    TestRunner::GetCurrentTestModule()->GetTestResponseProxy().ModuleDepends(moduleName, dependencyList);
}
static void int_trp_query_interface(uint32_t interface_id, void **outPtr) {
    TestRunner::GetCurrentTestModule()->GetTestResponseProxy().QueryInterface(interface_id, outPtr);
}

//
// V1 trampoline versions
//
static void int_trp_hook_precase_v1(TRUN_PRE_POST_HOOK_DELEGATE_V1 cbPreCase) {
    CBPrePostHook hook;
    hook.cbHookV1 = cbPreCase;
    TestRunner::GetCurrentTestModule()->GetTestResponseProxy().SetPreCaseCallback(hook);
}
static void int_trp_hook_postcase_v1(TRUN_PRE_POST_HOOK_DELEGATE_V1 cbPostCase) {
    CBPrePostHook hook;
    hook.cbHookV1 = cbPostCase;
    TestRunner::GetCurrentTestModule()->GetTestResponseProxy().SetPostCaseCallback(hook);
}



// Taken from another project...
#define TRUN_ARRAY_LENGTH(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

static TRUN_ConfigItem *get_config_items(size_t *nItems) {
    // This should wrap into the Config::instance instead...
    static TRUN_ConfigItem glb_Config[1] = {};
/*
    static TRUN_ConfigItem glb_Config[] = {
            {
                    .isValid = true,
                    .name = "item1",
                    .value_type = kTRCfgType_Num,
                    .value {
                            .num = 1,
                    }
            },
            {
                    .isValid = true,
                    .name = "item2",
                    .value_type = kTRCfgType_Bool,
                    .value {
                            .boolean = false,
                    }
            },
            {
                    .isValid = true,
                    .name = "discardTestReturnCode",
                    .value_type = kTRCfgType_Bool,
                    .value {
                            .boolean = Config::Instance().discardTestReturnCode,
                    }
            },
            {
                    .isValid = true,
                    .name = "item5",
                    .value_type = kTRCfgType_Num,
                    .value {
                            .num = 4711,
                    }
            },
    };
    */
    if (nItems != NULL) {
        *nItems = TRUN_ARRAY_LENGTH(glb_Config);
    }
    return glb_Config;
}

////
static size_t int_tcfg_list(size_t maxItems, TRUN_ConfigItem *outArray) {
    // Return number of items available

    size_t nGlbItems = 0;
    auto glb_config = get_config_items(&nGlbItems);

    if (outArray == nullptr) {
        return nGlbItems;
    }

    size_t nToCopy = (maxItems<nGlbItems)?maxItems:nGlbItems;
    for(size_t i=0;i<nToCopy;i++) {
        outArray[i] = glb_config[i];
    }
    return nToCopy;
}

static void int_tcfg_get(const char *key, TRUN_ConfigItem *outValue) {
    if (key == nullptr) {
        return;
    }
    if (outValue == nullptr) {
        return;
    }
    // test test
    if (key == std::string("continue_on_assert")) {
        outValue->isValid = true;
        strncpy(outValue->name, key, TR_CFG_ITEM_NAME_LEN-1);
        outValue->value_type = kTRCfgType_Bool;
        outValue->value.boolean = Config::Instance().continueOnAssert;
        return;
    }


    // TODO: Implement properly...
    if (key != std::string("enableThreadTestExecution")) {
        return;
    }
    outValue->isValid = true;
    strncpy(outValue->name,  "enableThreadTestExecution", TR_CFG_ITEM_NAME_LEN-1);
    outValue->value_type = kTRCfgType_Bool;
//    outValue->value.boolean = Config::Instance().enableThreadTestExecution;
    outValue->value.boolean = true;
}

// CITestingCoverage_IFace_ID
static void int_tcov_begincov(const char *symbol) {
    printf("BeginCoverage called for '%s'\n", symbol);
    if (!Config::Instance().isCoverageRunning) {
        printf("TCOV is not running!");
        return;
    }
    printf("Connecting to TCOV via '%s'\n", Config::Instance().coverageIPCName.c_str());
    gnilk::IPCFifoUnix ipc;
    if (!ipc.ConnectTo(Config::Instance().coverageIPCName)) {
        printf("Failed to connect to IPC using '%s'\n", Config::Instance().coverageIPCName.c_str());
        return;
    }

    CovIPCCmdMsg cmdMsg;
    cmdMsg.symbolName = symbol;

    gnilk::IPCBufferedWriter bufferedWriter(ipc);
    gnilk::IPCBinaryEncoder encoder(bufferedWriter);

    cmdMsg.Marshal(encoder);
    auto nFlush = bufferedWriter.Flush();
    printf("flushed fifo, res=%d\n", nFlush);
    printf("Sending signal to parent!\n");
    raise(SIGUSR1);
    // Sleep a little - allow tcov to catch up - should be plenty enough
    // The IPC should hold the data even if we are closed...
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // this should only close my end!
    ipc.Close();
    printf("Finished 'int_tcov_begincov'\n");
}