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

    public:
        static Config &Instance();
        ~Config() = default;

        std::string ResolveCacheDir();
        bool IsTrunTarget() const;

    protected:
        Config() = default;
    public:
        bool internal_test_startup = false;
        std::string version = TCOV_VERSION;
        std::string cache_dir = {};     // empty from start
        std::string description = "Calculating code coverage through LLDB";
        std::string target = "trun";
        std::string lcovReportFilename = "lcov.info";
        std::string diffReportFilename = "tcov_coverage.diff";
        std::vector<std::string> reportEngines = {"diff"};
        bool diffClean = false;
        int tab_size = 4;
        int verbose = 0;
        std::string ipc_name = "tcov-ipc";
        std::string lldb_server_path = "/usr/lib/llvm-18/bin/lldb-server";
        std::string symbolString = {};
        std::vector<CoverageSymbol> symbols = {};
        std::vector<std::string> target_args = {};
    };
}

#endif //TESTRUNNER_COVERAGE_CONFIG_H