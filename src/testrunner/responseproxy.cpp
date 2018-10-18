//
// TestResponseProxy - implements C/C++ calling convention wrappers for test reponses
//
// In order to support parallell testing (i.e. multiple threads) gateways are created on a per thread
// basis through the "GetInstance" method. It is not possible to create this class on your own, neither
// should you ever need to..
//
//
#include "testinterface.h"
#include "responseproxy.h"
#include "logger.h"
#include "config.h"

#include <stdlib.h> // malloc
#include <pthread.h>
#include <map>
#include <string>

static void int_trp_debug(int line, const char *file, const char *format, ...);
static void int_trp_info(int line, const char *file, const char *format, ...);
static void int_trp_warning(int line, const char *file, const char *format, ...);
static void int_trp_error(int line, const char *file, const char *format, ...);
static void int_trp_fatal(int line, const char *file, const char *format, ...);
static void int_trp_abort(int line, const char *file, const char *format, ...);

// Holds a calling proxy per thread
static std::map<pthread_t, TestResponseProxy *> trpLookup;

//
// TODO: Need to update the logger to allow specific loggers to always pass through messages
//
TestResponseProxy::TestResponseProxy() {
    this->trp = (ITesting *)malloc(sizeof(ITesting));
    this->trp->Debug = int_trp_debug;
    this->trp->Info = int_trp_info;
    this->trp->Warning = int_trp_warning;
    this->trp->Error = int_trp_error;
    this->trp->Fatal = int_trp_fatal;
    this->trp->Abort = int_trp_abort;
}

void TestResponseProxy::Begin(std::string symbolName, std::string moduleName) {
    this->symbolName = symbolName;
    this->moduleName = moduleName;
    errorCount = 0;
    testResult = kTestResult_Pass;
    pLogger = gnilk::Logger::GetLogger(moduleName.c_str());

    // Apply verbose filtering to log output from test cases or not??
    if (!Config::Instance()->testLogFilter) {
        pLogger->Enable(gnilk::Logger::kFlags_PassThrough);
    } else {
        pLogger->Enable(gnilk::Logger::kFlags_BlockAll);
    } 
    

    timer.Reset();
}

double TestResponseProxy::ElapsedTimeInSec() {
    return tElapsed;
}

void TestResponseProxy::End() {
    tElapsed = timer.Sample();
    symbolName.clear();    
    moduleName.clear();
    pLogger = NULL;
}

int TestResponseProxy::Errors() {
    return errorCount;
}

kTestResult TestResponseProxy::Result() {
    return testResult;
}



// ITesting mirror
void TestResponseProxy::Debug(int line, const char *file, std::string message) {
    //gnilk::ILogger *pLogger = gnilk::Logger::GetLogger(moduleName.c_str());
    pLogger->Debug("%s:%d:%s", file, line, message.c_str());
}
void TestResponseProxy::Info(int line, const char *file, std::string message) {
    //gnilk::ILogger *pLogger = gnilk::Logger::GetLogger(moduleName.c_str());
    pLogger->Info("%s:%d:%s", file, line, message.c_str());
}
void TestResponseProxy::Warning(int line, const char *file, std::string message) {
    //gnilk::ILogger *pLogger = gnilk::Logger::GetLogger(moduleName.c_str());
    pLogger->Warning("%s:%d:%s", file, line, message.c_str());
}


void TestResponseProxy::Error(int line, const char *file, std::string message) {
    //gnilk::ILogger *pLogger = gnilk::Logger::GetLogger(moduleName.c_str());
    pLogger->Error("%s:%d:%s", file, line, message.c_str());
    this->errorCount++;
    if (testResult < kTestResult_TestFail) {
        testResult = kTestResult_TestFail;
    }
}

void TestResponseProxy::Fatal(int line, const char *file, std::string message) {
    //gnilk::ILogger *pLogger = gnilk::Logger::GetLogger(symbolName.c_str());
    pLogger->Critical("%s:%d: %s", file, line, message.c_str());
    this->errorCount++;
    if (testResult < kTestResult_ModuleFail) {
        testResult = kTestResult_ModuleFail;
    }
}

void TestResponseProxy::Abort(int line, const char *file, std::string message) {
    //gnilk::ILogger *pLogger = gnilk::Logger::GetLogger(symbolName.c_str());
    pLogger->Fatal("%s:%d: %s", file, line, message.c_str());
    this->errorCount++;
    if (testResult < kTestResult_AllFail) {
        testResult = kTestResult_AllFail;
    }

}



//
// GetInstance - returns a test proxy, if one does not exists for the thread one will be allocated
//
TestResponseProxy *TestResponseProxy::GetInstance() {
    pthread_t tid = pthread_self();

    gnilk::ILogger *pLogger = gnilk::Logger::GetLogger("TestResponseProxy");

    if (trpLookup.find(tid) == trpLookup.end()) {
        pLogger->Debug("GetInstance, allocating new instance with tid: 0x%.8x", tid);
        TestResponseProxy *trp = new TestResponseProxy();
        trpLookup.insert(std::pair<pthread_t, TestResponseProxy *>(tid, trp));
        return trp;
    } else {
        //pLogger->Debug("GetInstance, returning existing proxy instance for tid: 0x%.8x", tid);
    }
    return trpLookup[tid];

}

//
// wrappers for pure C call's (no this) - only one call per thread allowed.
//

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


static bool IsMsgSizeOk(uint32_t szbuf) {
    if (szbuf > Config::Instance()->responseMsgByteLimit) {
        gnilk::ILogger *pLogger = gnilk::Logger::GetLogger("TestResponseProxy");    
        pLogger->Error("Message buffer exceeds limit (%d bytes), truncating..");
        return false;
    }
    return true;
}

static void int_trp_debug(int line, const char *file, const char *format, ...) {
    CREATE_REPORT_STRING()
    TestResponseProxy *trp = TestResponseProxy::GetInstance();
    trp->Debug(line, file, std::string(newstr));
}
static void int_trp_info(int line, const char *file, const char *format, ...) {
    CREATE_REPORT_STRING()
    TestResponseProxy *trp = TestResponseProxy::GetInstance();
    trp->Info(line, file, std::string(newstr));
}
static void int_trp_warning(int line, const char *file, const char *format, ...) {
    CREATE_REPORT_STRING()
    TestResponseProxy *trp = TestResponseProxy::GetInstance();
    trp->Warning(line, file, std::string(newstr));
}
static void int_trp_error(int line, const char *file, const char *format, ...) {
    CREATE_REPORT_STRING()
    TestResponseProxy *trp = TestResponseProxy::GetInstance();
    trp->Error(line, file, std::string(newstr));
}

static void int_trp_fatal(int line, const char *file, const char *format, ...) {
    CREATE_REPORT_STRING()
    TestResponseProxy *trp = TestResponseProxy::GetInstance();
    trp->Fatal(line, file, std::string(newstr));
}

static void int_trp_abort(int line, const char *file, const char *format, ...) {
    CREATE_REPORT_STRING()
    TestResponseProxy *trp = TestResponseProxy::GetInstance();
    trp->Abort(line,file, std::string(newstr));
}


#undef CREATE_REPORT_STRING