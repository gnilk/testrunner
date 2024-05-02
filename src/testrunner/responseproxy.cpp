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

#include <string.h>
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
static void int_trp_moduledepend(const char *moduleName, const char *dependencyList);
static void int_trp_query_interface(uint32_t interface_id, void **ouPtr);

// Config interface
static size_t int_tcfg_list(size_t maxItems, TRUN_ConfigItem *outArray);
static void int_tcfg_get(const char *key, TRUN_ConfigItem *outValue);

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

    // Note: can't have this in thread-local, each tests will still run in it's own isolated thread..
#ifdef TRUN_HAVE_THREADS
    static TestResponseProxy glbResponseProxy;
#else
    static TestResponseProxy glbResponseProxy;
#endif
    return glbResponseProxy;
}


//
TestResponseProxy::TestResponseProxy() {
}

void TestResponseProxy::Begin(const std::string &use_symbolName, const std::string &use_moduleName) {
    symbolName = use_symbolName;
    moduleName = use_moduleName;
    errorCount = 0;
    assertCount = 0;
    testResult = kTestResult_Pass;
    pLogger = gnilk::Logger::GetLogger("TestResponseProxy");

    trp = TestResponseProxy::GetTRTestInterface();

    // Apply verbose filtering to log output from test cases or not??
    // This was part of the old logging library but not the new one..
#if 0
    // Not supported on gnklog - not sure if this was a feature or if I hacked it in for this project...
    if (!Config::Instance().testLogFilter) {
        pLogger->Enable(Logger::kFlags_PassThrough);
    } else {
        pLogger->Enable(Logger::kFlags_BlockAll);
    } 
#endif

    timer.Reset();
    assertError.Reset();
}

void TestResponseProxy::End() {
    tElapsed = timer.Sample();
    symbolName.clear();     // No?
    moduleName.clear();
    pLogger = nullptr;
}


// Consider moving this out of here
ITesting *TestResponseProxy::GetTRTestInterface() {
    static ITesting trp_bridge = {
            .Debug = int_trp_debug,
            .Info = int_trp_info,
            .Warning = int_trp_warning,
            .Error = int_trp_error,
            .Fatal = int_trp_fatal,
            .Abort = int_trp_abort,
            .AssertError = int_trp_assert_error,
            .SetPreCaseCallback = int_trp_hook_precase,
            .SetPostCaseCallback = int_trp_hook_postcase,
            .CaseDepends = int_trp_casedepend,
            .ModuleDepends = int_trp_moduledepend,
            .QueryInterface = int_trp_query_interface,
    };
    return &trp_bridge;
}

// Move to other file
ITestingConfig *TestResponseProxy::GetTRConfigInterface() {
    static ITestingConfig trp_config_bridge = {
            .List = int_tcfg_list,
            .Get = int_tcfg_get,
    };
    return &trp_config_bridge;
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
    assertError.Set(AssertError::kAssert_Fatal, line, file, message);
    TerminateThreadIfNeeded();
}

void TestResponseProxy::Abort(int line, const char *file, std::string message) {
    pLogger->Debug("Abort Error: %s:%d\t'%s'", file, line, message.c_str());

    printf("*** ABORT ERROR: %s:%d\t'%s'\n", file, line, message.c_str());
    this->errorCount++;
    if (testResult < kTestResult_AllFail) {
        testResult = kTestResult_AllFail;
    }
    assertError.Set(AssertError::kAssert_Abort, line, file, message);
    TerminateThreadIfNeeded();
}

//void (*AssertError)(const char *exp, const char *file, const int line);
void TestResponseProxy::AssertError(const char *exp, const char *file, const int line) {
    pLogger->Debug("Assert Error: %s:%d\t'%s'", file, line, exp);
    printf("*** ASSERT ERROR: %s:%d\t'%s'\n", file, line, exp);

    this->assertCount++;
    if (testResult < kTestResult_TestFail) {
        testResult = kTestResult_TestFail;
    }
    assertError.Set(AssertError::kAssert_Error, line, file, exp);
    TerminateThreadIfNeeded();
}
void TestResponseProxy::TerminateThreadIfNeeded() {
#ifdef TRUN_HAVE_THREADS
    if (Config::Instance().enableThreadTestExecution) {
        #ifdef WIN32
            TerminateThread(GetCurrentThread(), 0);
        #else
            pthread_exit(NULL);
        #endif
    }
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
        testModule->AddDependencyForCase(caseName, dependencyList);
    }
}

// FIXME: Implement
void TestResponseProxy::ModuleDepends(const char *moduleName, const char *dependencyList) {
    pLogger->Debug("NOT YET IMPLEMENTED");
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
        default :
            *outPtr = nullptr;
            pLogger->Debug("QueryInterface, invalid interface id (%d)", interface_id);
            break;
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
        auto pLogger = gnilk::Logger::GetLogger("TestResponseProxy");
        pLogger->Error("Message buffer exceeds limit (%d bytes), truncating..");
        return false;
    }
    return true;
}

static void int_trp_debug(int line, const char *file, const char *format, ...) {
    CREATE_REPORT_STRING()
    TestRunner::HACK_GetCurrentTestModule()->GetTestResponseProxy().Debug(line, file, std::string(newstr));
    //TestResponseProxy::Instance().Debug(line, file, std::string(newstr));
}
static void int_trp_info(int line, const char *file, const char *format, ...) {
    CREATE_REPORT_STRING()
    TestRunner::HACK_GetCurrentTestModule()->GetTestResponseProxy().Info(line, file, std::string(newstr));
//    TestResponseProxy::Instance().Info(line, file, std::string(newstr));
}
static void int_trp_warning(int line, const char *file, const char *format, ...) {
    CREATE_REPORT_STRING()
    TestRunner::HACK_GetCurrentTestModule()->GetTestResponseProxy().Warning(line, file, std::string(newstr));
//    TestResponseProxy::Instance().Warning(line, file, std::string(newstr));
}
static void int_trp_error(int line, const char *file, const char *format, ...) {
    CREATE_REPORT_STRING()
    TestRunner::HACK_GetCurrentTestModule()->GetTestResponseProxy().Error(line, file, std::string(newstr));
//    TestResponseProxy::Instance().Error(line, file, std::string(newstr));
}

static void int_trp_fatal(int line, const char *file, const char *format, ...) {
    CREATE_REPORT_STRING()
    TestRunner::HACK_GetCurrentTestModule()->GetTestResponseProxy().Fatal(line, file, std::string(newstr));
//    TestResponseProxy::Instance().Fatal(line, file, std::string(newstr));
}

static void int_trp_abort(int line, const char *file, const char *format, ...) {
    CREATE_REPORT_STRING()
    TestRunner::HACK_GetCurrentTestModule()->GetTestResponseProxy().Abort(line, file, std::string(newstr));
    //TestResponseProxy::Instance().Abort(line,file, std::string(newstr));
}

static void int_trp_assert_error(const char *exp, const char *file, int line) {
    TestRunner::HACK_GetCurrentTestModule()->GetTestResponseProxy().AssertError(exp, file, line);
    //TestResponseProxy::Instance().AssertError(exp, file, line);
}

#undef CREATE_REPORT_STRING

static void int_trp_hook_precase(TRUN_PRE_POST_HOOK_DELEGATE cbPreCase) {
    TestRunner::HACK_GetCurrentTestModule()->GetTestResponseProxy().SetPreCaseCallback(cbPreCase);
    //TestResponseProxy::Instance().SetPreCaseCallback(cbPreCase);
}

static void int_trp_hook_postcase(TRUN_PRE_POST_HOOK_DELEGATE cbPostCase) {
    TestRunner::HACK_GetCurrentTestModule()->GetTestResponseProxy().SetPostCaseCallback(cbPostCase);
    //TestResponseProxy::Instance().SetPostCaseCallback(cbPostCase);
}

static void int_trp_casedepend(const char *caseName, const char *dependencyList) {
    TestRunner::HACK_GetCurrentTestModule()->GetTestResponseProxy().CaseDepends(caseName, dependencyList);
    //TestResponseProxy::Instance().CaseDepends(caseName, dependencyList);
}
static void int_trp_moduledepend(const char *moduleName, const char *dependencyList) {
    TestRunner::HACK_GetCurrentTestModule()->GetTestResponseProxy().ModuleDepends(moduleName, dependencyList);
}



static void int_trp_query_interface(uint32_t interface_id, void **outPtr) {
    TestRunner::HACK_GetCurrentTestModule()->GetTestResponseProxy().QueryInterface(interface_id, outPtr);
//    return TestResponseProxy::Instance().QueryInterface(interface_id, outPtr);
}


// Taken from another project...
#define TRUN_ARRAY_LENGTH(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

static TRUN_ConfigItem *get_config_items(size_t *nItems) {
    // This should wrap into the Config::instance instead...
    static TRUN_ConfigItem glb_Config[] {
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
                    .name = "enableThreadTestExecution",
                    .value_type = kTRCfgType_Bool,
                    .value {
                            .boolean = Config::Instance().enableThreadTestExecution,
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
    // TODO: Implement properly...
    if (key != std::string("enableThreadTestExecution")) {
        return;
    }
    outValue->isValid = true;
    strncpy(outValue->name,  "enableThreadTestExecution", TR_CFG_ITEM_NAME_LEN-1);
    outValue->value_type = kTRCfgType_Bool;
    outValue->value.boolean = Config::Instance().enableThreadTestExecution;
}
