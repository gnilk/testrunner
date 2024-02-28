//
// Created by gnilk on 21.09.22.
//

#ifndef TESTRUNNER_REPORTJSONEXT_H
#define TESTRUNNER_REPORTJSONEXT_H

#include "reportjson.h"

namespace trun {

   class ResultsReportJSONExtensive : public ResultsReportJSON {
    public:
        void Begin() override;
        void End() override;
        void PrintReport() override;
    protected:

    private:
        void PrintFuncResult(const TestFunc::Ref &function);

        void DumpNewStruct();
    };
}

#endif //TESTRUNNER_REPORTJSONEXT_H
