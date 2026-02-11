//
// Created by gnilk on 11.02.2026.
//

#ifndef TESTRUNNER_REPORTBASE_H
#define TESTRUNNER_REPORTBASE_H

#include "../Breakpoint.h"

namespace tcov {
    class ReportBase {
    public:
        ReportBase() = default;
        virtual ~ReportBase() = default;

        virtual void GenerateReport(const BreakpointManager &breakpoints) {}

    };
}

#endif //TESTRUNNER_REPORTBASE_H