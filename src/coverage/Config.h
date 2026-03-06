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
        static Config &Instance();
        ~Config() = default;


    protected:
        Config() = default;
    public:
        std::string target = "trun";
        bool verbose = false;
        std::string ipc_name = "tcov-ipc";
        std::string symbolString = {};
        std::vector<std::string> symbols = {};
        std::vector<std::string> target_args = {};
    };
}

#endif //TESTRUNNER_COVERAGE_CONFIG_H