//
// Created by gnilk on 11/21/2022.
//

#ifndef TESTRUNNER_TRUNEMBEDDED_H
#define TESTRUNNER_TRUNEMBEDDED_H

#include "testinterface.h"
#include <string>

namespace trun {

    extern "C" {
        typedef int (*PTESTCASE)(ITesting *param);
    }

    // Macro to simplify adding test-cases on embedded - natively you don't need this as we resolve symbols in runtime
    #define TRUN_ADD_TEST(_TC_) do { trun::AddTestCase(#_TC_, _TC_); } while(0)

    void Initialize();
    void AddTestCase(const char *symbolName, PTESTCASE func);
    void RunTests(const char *moduleFilter, const char *caseFilter);
}

#endif //TESTRUNNER_TRUNEMBEDDED_H
