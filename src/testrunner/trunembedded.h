//
// Created by gnilk on 11/21/2022.
//

#ifndef TESTRUNNER_TRUNEMBEDDED_H
#define TESTRUNNER_TRUNEMBEDDED_H

#include <string>
#include "dynlib.h"


namespace trun {
    void Initialize();
    void AddTestCase(std::string symbolName, PTESTFUNC func);
    void RunTests();
}

#endif //TESTRUNNER_TRUNEMBEDDED_H
