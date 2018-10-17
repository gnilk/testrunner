#include "testfunc.h"
#include "module.h"
#include "config.h"
#include "testrunner.h"
#include <string>
#include <pthread.h>
#include <map>

//
// WIP - test interface per thread
//

static void int_tgw_error(int code);
static std::map<pthread_t, TestGW *> tgwLookup;
TestGW *GetTestGW();

TestGW::TestGW() {
    this->tgw = (ITesting *)malloc(sizeof(ITesting));
    this->tgw->Error = int_tgw_error;
}

void TestGW::Error(int code) {
    printf("TestGW::Error: %d\n", code);
}

static void int_tgw_error(int code) {
    //
    // Need: std::map<pthread_id(),TestGW *> tgwLoopkup;
    //
    TestGW *tgw = GetTestGW();
    printf("** TID: 0x%.8x, int_tgw_error: %d\n", pthread_self(), code);
    tgw->Error(code);
}

//
// GetTestGW - returns a test gateway, if one does not exists for the thread one will be allocated
//
TestGW *GetTestGW() {
    pthread_t tid = pthread_self();

    if (tgwLookup.find(tid) == tgwLookup.end()) {
        printf("\n** GetTestGW, allocating with tid: 0x%.8x\n", tid);
        TestGW *tgw = new TestGW();
        tgwLookup.insert(std::pair<pthread_t, TestGW *>(tid, tgw));
        return tgw;
    } else {
        printf("\n** GetTestGW, return existing (tid: 0x%.8x)\n", tid);
    }
    return tgwLookup[tid];

}
//
// WIP - End of test interface per therad
//


TestFunc::TestFunc() {
    isExecuted = false;
    pLogger = gnilk::Logger::GetLogger("TestFunc");
}
TestFunc::TestFunc(std::string symbolName, std::string moduleName, std::string caseName) {
    this->symbolName = symbolName;
    this->moduleName = moduleName;
    this->caseName = caseName;
    isExecuted = false;
    pLogger = gnilk::Logger::GetLogger("TestFunc");
}

bool TestFunc::IsGlobal() {
    return (moduleName == "-");
}
bool TestFunc::IsGlobalMain() {
    return (IsGlobal() && (caseName == Config::Instance()->testMain));
}
void TestFunc::Execute(IModule *module) {
    pLogger->Debug("Executing test: %s", caseName.c_str());
    pLogger->Debug("  Module: %s", moduleName.c_str());
    pLogger->Debug("  Case..: %s", caseName.c_str());
    pLogger->Debug("  Export: %s", symbolName.c_str());

    // ?Executed, issue warning, but proceed..
    if (Executed()) {
        pLogger->Warning("Test '%s' already executed - double execution is either bug or not advised!!", symbolName.c_str());
    }

    PTESTFUNC pFunc = (PTESTFUNC)module->FindExportedSymbol(symbolName);
    if (pFunc != NULL) {
        
        pFunc((void *)GetTestGW()->Gateway());
    }
    SetExecuted();

}

void TestFunc::SetExecuted() {
    isExecuted = true;
}

bool TestFunc::Executed() {
    return isExecuted;
}
