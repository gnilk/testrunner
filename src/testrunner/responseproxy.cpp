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

static void int_trp_error(const char *format, ...);
static void int_trp_fatal(const char *format, ...);
static void int_trp_abort(const char *format, ...);

static std::map<pthread_t, TestResponseProxy *> trpLookup;

//
// TODO: Need to update the logger to allow specific loggers to always pass through messages
//
TestResponseProxy::TestResponseProxy() {
    this->trp = (ITesting *)malloc(sizeof(ITesting));
    this->trp->Error = int_trp_error;
    this->trp->Fatal = int_trp_fatal;
    this->trp->Abort = int_trp_abort;
}

void TestResponseProxy::Begin(std::string symbolName) {
    symbolName = symbolName;
    errorCount = 0;
    testResult = kTestResult_Pass;
    gnilk::ILogger *pLogger = gnilk::Logger::GetLogger(symbolName.c_str());
    timer.Reset();
}

double TestResponseProxy::ElapsedTimeInSec() {
    return tElapsed;
}

void TestResponseProxy::End() {
    tElapsed = timer.Sample();
    symbolName.clear();    
}

int TestResponseProxy::Errors() {
    return errorCount;
}

kTestResult TestResponseProxy::Result() {
    return testResult;
}



// ITesting mirror


void TestResponseProxy::Error(std::string message) {
    gnilk::ILogger *pLogger = gnilk::Logger::GetLogger(symbolName.c_str());
    pLogger->Error("%s", message.c_str());
    this->errorCount++;
    if (testResult < kTestResult_TestFail) {
        testResult = kTestResult_TestFail;
    }
}

void TestResponseProxy::Fatal(std::string message) {
    gnilk::ILogger *pLogger = gnilk::Logger::GetLogger(symbolName.c_str());
    pLogger->Critical("%s", message.c_str());
    this->errorCount++;
    if (testResult < kTestResult_ModuleFail) {
        testResult = kTestResult_ModuleFail;
    }
}

void TestResponseProxy::Abort(std::string message) {
    gnilk::ILogger *pLogger = gnilk::Logger::GetLogger(symbolName.c_str());
    pLogger->Critical("%s", message.c_str());
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
        pLogger->Debug("GetInstance, allocating new instance with tid: 0x%.8x\n", tid);
        TestResponseProxy *trp = new TestResponseProxy();
        trpLookup.insert(std::pair<pthread_t, TestResponseProxy *>(tid, trp));
        return trp;
    } else {
        pLogger->Debug("GetInstance, returning existing proxy instance for tid: 0x%.8x\n", tid);
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

static void int_trp_error(const char *format, ...) {
    CREATE_REPORT_STRING()

    TestResponseProxy *trp = TestResponseProxy::GetInstance();
    trp->Error(std::string(newstr));
}

static void int_trp_fatal(const char *format, ...) {
    CREATE_REPORT_STRING()
    TestResponseProxy *trp = TestResponseProxy::GetInstance();
    trp->Fatal(std::string(newstr));
}

static void int_trp_abort(const char *format, ...) {
    CREATE_REPORT_STRING()
    TestResponseProxy *trp = TestResponseProxy::GetInstance();
    trp->Abort(std::string(newstr));
}


#undef CREATE_REPORT_STRING