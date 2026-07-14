//
// tcov_utests - Config::IsTrunTarget detection tests (§5 / Phase 3).
//
// trun auto-detection keys on the executable BASENAME (a substring match
// false-positives on any path containing "trun" - e.g. 'trunembedded', or any target
// under a '.../testrunner/...' directory), with explicit --trun / --no-trun overrides.
//
#include "ext_testinterface/testinterface.h"
// Relative path on purpose: the tests target also has src/testrunner on the include
// path (for ext_testinterface/), where a different "Config.h" (trun's) lives. Reach the
// coverage Config unambiguously.
#include "../Config.h"

using namespace tcov;

extern "C" {
    DLL_EXPORT int test_config(ITesting *t);
    DLL_EXPORT int test_config_exit(ITesting *t);
    DLL_EXPORT int test_config_istrun_basename(ITesting *t);
    DLL_EXPORT int test_config_istrun_override(ITesting *t);
}

DLL_EXPORT int test_config(ITesting *t) {
    return kTR_Pass;
}

// Config is a shared singleton - restore it to defaults so case ordering can't leak state.
DLL_EXPORT int test_config_exit(ITesting *t) {
    Config::Instance().target = "trun";
    Config::Instance().trunDetect = Config::TrunDetect::kAuto;
    return kTR_Pass;
}

// Auto-detect matches the executable BASENAME exactly, not a substring.
DLL_EXPORT int test_config_istrun_basename(ITesting *t) {
    auto &cfg = Config::Instance();
    cfg.trunDetect = Config::TrunDetect::kAuto;

    // basename == "trun" -> trun
    cfg.target = "trun";         TR_ASSERT(t, cfg.IsTrunTarget());
    cfg.target = "./trun";       TR_ASSERT(t, cfg.IsTrunTarget());
    cfg.target = "/a/b/trun";    TR_ASSERT(t, cfg.IsTrunTarget());

    // substring "trun" but basename != "trun" -> NOT trun (the substring false positives)
    cfg.target = "trunembedded";        TR_ASSERT(t, !cfg.IsTrunTarget());
    cfg.target = "/x/testrunner/app";   TR_ASSERT(t, !cfg.IsTrunTarget());
    cfg.target = "mytrun";              TR_ASSERT(t, !cfg.IsTrunTarget());
    cfg.target = "/opt/trunk/bin/app";  TR_ASSERT(t, !cfg.IsTrunTarget());
    cfg.target = "";                    TR_ASSERT(t, !cfg.IsTrunTarget());

    return kTR_Pass;
}

// Explicit overrides win over the basename heuristic.
DLL_EXPORT int test_config_istrun_override(ITesting *t) {
    auto &cfg = Config::Instance();

    // --trun forces trun even for a renamed binary
    cfg.trunDetect = Config::TrunDetect::kForceTrun;
    cfg.target = "mytrun";       TR_ASSERT(t, cfg.IsTrunTarget());
    cfg.target = "/x/whatever";  TR_ASSERT(t, cfg.IsTrunTarget());

    // --no-trun forces generic even for a binary literally named "trun"
    cfg.trunDetect = Config::TrunDetect::kForceGeneric;
    cfg.target = "trun";         TR_ASSERT(t, !cfg.IsTrunTarget());
    cfg.target = "/a/b/trun";    TR_ASSERT(t, !cfg.IsTrunTarget());

    return kTR_Pass;
}
