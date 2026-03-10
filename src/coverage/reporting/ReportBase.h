//
// Created by gnilk on 11.02.2026.
//

#ifndef TESTRUNNER_REPORTBASE_H
#define TESTRUNNER_REPORTBASE_H

#include <string>
#include "../Breakpoint.h"

namespace tcov {
    class ReportBase {
    public:
        ReportBase() = default;
        virtual ~ReportBase() = default;

        virtual void GenerateReport(const BreakpointManager &breakpoints) {}

        // Make this a generic reporting function
        static std::string GetShortDisplayName(const std::string &sbname) {
            auto shortName = sbname.substr(0, sbname.find('('));
            return shortName;
        }

    };
}

#endif //TESTRUNNER_REPORTBASE_H