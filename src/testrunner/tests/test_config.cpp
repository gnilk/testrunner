//
// Unit tests for Config::FromArguments command-line parsing.
//
// Covers two regressions in the ArgParser-based parser (config.cpp):
//   * '-t <case>' without '-m' must only populate the test-case filter, not the
//     module filter (a bare '-t' previously ran nothing).
//   * '-d' (dump config) and '-D' (disable Linux RTLD_DEEPBIND) must be two
//     independent flags (they previously collided on '-d').
//
#include "../testinterface_internal.h"
#include "../config.h"
#include <vector>
#include <string>

using namespace trun;

extern "C" {
    DLL_EXPORT int test_config(ITesting *t);
    DLL_EXPORT int test_config_twithoutm(ITesting *t);
    DLL_EXPORT int test_config_dumponly(ITesting *t);
    DLL_EXPORT int test_config_deepbind(ITesting *t);
    DLL_EXPORT int test_config_emptyfilter(ITesting *t);
}

DLL_EXPORT int test_config(ITesting *t) {
    return kTR_Pass;
}

// '-t <case>' without '-m' should only set the test-case filter and leave the
// module filter at its match-all default ('-'). Previously '-t' was also fed
// into the module filter (which replaces the list), so a bare '-t' filtered out
// every module and ran nothing.
DLL_EXPORT int test_config_twithoutm(ITesting *t) {
    Config saved = Config::Instance();

    const char *argv[] = {"trun", "-t", "split", "lib.so"};
    auto res = Config::FromArguments(4, argv);

    auto modules = Config::Instance().modules;
    auto testcases = Config::Instance().testcases;

    Config::Instance() = saved;

    TR_ASSERT(t, res == Config::FromArgRes::kSuccess);
    // module filter must stay at the match-all default
    TR_ASSERT(t, modules.size() == 1);
    TR_ASSERT(t, modules[0] == "-");
    // test-case filter must hold the requested case
    TR_ASSERT(t, testcases.size() == 1);
    TR_ASSERT(t, testcases[0] == "split");
    return kTR_Pass;
}

// An all-separator/whitespace filter ('-t ,,,') splits to zero usable entries.
// It must NOT overwrite the match-all default and silently run nothing - the
// filter is ignored (with a stderr warning) and '-' is left in place.
DLL_EXPORT int test_config_emptyfilter(ITesting *t) {
    Config saved = Config::Instance();

    const char *argv[] = {"trun", "-t", ",,,", "-m", "  ", "lib.so"};
    auto res = Config::FromArguments(6, argv);

    auto modules = Config::Instance().modules;
    auto testcases = Config::Instance().testcases;

    Config::Instance() = saved;

    TR_ASSERT(t, res == Config::FromArgRes::kSuccess);
    // both filters must stay at the match-all default rather than becoming empty
    TR_ASSERT(t, testcases.size() == 1);
    TR_ASSERT(t, testcases[0] == "-");
    TR_ASSERT(t, modules.size() == 1);
    TR_ASSERT(t, modules[0] == "-");
    return kTR_Pass;
}

// '-d' should only request the configuration dump and must NOT change Linux
// deep-binding behaviour (that is the separate '-D' flag).
DLL_EXPORT int test_config_dumponly(ITesting *t) {
    Config saved = Config::Instance();

    const char *argv[] = {"trun", "-d", "lib.so"};
    auto res = Config::FromArguments(3, argv);

    bool dumpConfig = Config::Instance().dumpConfig;
    bool useDeepBinding = Config::Instance().linuxUseDeepBinding;

    Config::Instance() = saved;

    TR_ASSERT(t, res == Config::FromArgRes::kSuccess);
    TR_ASSERT(t, dumpConfig == true);
    TR_ASSERT(t, useDeepBinding == true);
    return kTR_Pass;
}

// '-D' should disable Linux deep-binding and must NOT request the config dump.
DLL_EXPORT int test_config_deepbind(ITesting *t) {
    Config saved = Config::Instance();

    const char *argv[] = {"trun", "-D", "lib.so"};
    auto res = Config::FromArguments(3, argv);

    bool dumpConfig = Config::Instance().dumpConfig;
    bool useDeepBinding = Config::Instance().linuxUseDeepBinding;

    Config::Instance() = saved;

    TR_ASSERT(t, res == Config::FromArgRes::kSuccess);
    TR_ASSERT(t, dumpConfig == false);
    TR_ASSERT(t, useDeepBinding == false);
    return kTR_Pass;
}
