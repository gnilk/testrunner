//
// Created by gnilk on 11.02.2026.
//

#ifndef TESTRUNNER_REPORTCONSOLE_H
#define TESTRUNNER_REPORTCONSOLE_H

#include "ReportBase.h"

namespace tcov {
    class ReportConsole : public ReportBase {
    public:
        ReportConsole() = default;
        virtual ~ReportConsole() = default;

        void GenerateReport(const BreakpointManager &breakpoints) override;
    };
}


#endif //TESTRUNNER_REPORTCONSOLE_H