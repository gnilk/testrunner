//
// Created by gnilk on 10.03.2026.
//

#ifndef TESTRUNNER_REPORTLCOV_H
#define TESTRUNNER_REPORTLCOV_H

#include "ReportBase.h"

namespace tcov {
    class ReportLCOV : public ReportBase {
    public:
        ReportLCOV() = default;
        ~ReportLCOV() override = default;

        void GenerateReport(const BreakpointManager &breakpoints) override;
    };
}

#endif //TESTRUNNER_REPORTLCOV_H