//
// Created by gnilk on 20.03.2026.
//

#ifndef TESTRUNNER_REPORTDIFF_H
#define TESTRUNNER_REPORTDIFF_H

#include "ReportBase.h"

namespace tcov {
    class ReportDiff : public ReportBase {
    public:
        ReportDiff() = default;
        virtual ~ReportDiff() = default;

        void GenerateReport(const BreakpointManager &breakpoints) override;
    };
}


#endif //TESTRUNNER_REPORTDIFF_H