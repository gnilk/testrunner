//
// Created by Fredrik Kling on 17.08.22.
//

#include "resultsummary.h"
#include <stdio.h>

// Only one instance...
static ResultSummary *glb_Instance = nullptr;

void ResultSummary::PrintSummary(bool bPrintSuccess) {
    PrintFailureDetails();
    if (bPrintSuccess) {
        PrintPassDetails();
    }
}
void ResultSummary::PrintFailureDetails() {
    printf("Failed:\n");
    for(auto r : results) {
        if (r->Result() != kTestResult_Pass) {
            printf("  [%c%c%c]: %s\n",
                   r->Result() == kTestResult_TestFail?'T':'t',
                   r->Result() == kTestResult_ModuleFail?'M':'m',
                   r->Result() == kTestResult_AllFail?'A':'a',
                   r->SymbolName().c_str());
        }
    }
}
void ResultSummary::PrintPassDetails() {
    printf("Success:\n");
    for(auto r : results) {
        if (r->Result() == kTestResult_Pass) {
            printf("  [P--]: %s\n",r->SymbolName().c_str());
        }
    }
}

ResultSummary &ResultSummary::Instance() {
    if (glb_Instance == nullptr) {
        glb_Instance = new ResultSummary;
    }
    return *glb_Instance;
}