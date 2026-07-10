//
// Created by gnilk on 06.03.2026.
//

#ifndef TESTRUNNER_COVERAGE_CONFIG_H
#define TESTRUNNER_COVERAGE_CONFIG_H

#include <string>
#include <vector>

namespace tcov {
    class Config {
    public:
        struct CoverageSymbol {
            bool isGlob = false;
            std::string name = {};
            std::string globPrefix = {};
        };

        // How the target is classified as 'trun' (gates the dylib-load SIGUSR1 sync,
        // the --sequential/--coverage arg injection and signal trapping). Auto-detect
        // is basename-based; the two force modes are the --trun / --no-trun overrides.
        enum class TrunDetect {
            kAuto,          // basename(target) == "trun"
            kForceTrun,     // --trun     : treat as trun regardless of name
            kForceGeneric,  // --no-trun  : treat as a generic target regardless of name
        };

    public:
        static Config &Instance();
        ~Config() = default;

        bool IsTrunTarget() const;

    protected:
        Config() = default;
    public:
        bool internal_test_startup = false;
        std::string version = TCOV_VERSION;
        std::string description = "Calculating code coverage through LLDB";
        std::string target = "trun";
        TrunDetect trunDetect = TrunDetect::kAuto;
        std::string lcovReportFilename = "lcov.info";
        std::string diffReportFilename = "tcov_coverage.diff";
        std::vector<std::string> reportEngines = {"diff"};
        int tab_size = 4;
        int verbose = 0;
        std::string lldb_server_path = "/usr/lib/llvm-18/bin/lldb-server";
        std::string symbolString = {};
        std::vector<CoverageSymbol> symbols = {};
        std::vector<std::string> target_args = {};
    };
}

#endif //TESTRUNNER_COVERAGE_CONFIG_H