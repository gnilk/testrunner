//
// Created by gnilk on 09.05.24.
//
// Regression tests for the single filter matcher (trun::caseMatch). This used to be a
// scratchpad with a private copy of the matcher; it now pins the behaviour of the one shared
// matcher used by the listing and by both execution paths (module + case, sequential + fork).
//
#include "../config.h"
#include "../../shared/strutil.h"
#include "ext_testinterface/testinterface.h"
#include <vector>
#include <string>

extern "C" {
DLL_EXPORT int test_execorder(ITesting *t);
DLL_EXPORT int test_execorder_match(ITesting *t);
}

DLL_EXPORT int test_execorder(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_execorder_match(ITesting *t) {
    using V = std::vector<std::string>;

    // A positive glob matches EVERY name - the old per-arg module matcher stopped at the first
    // match (goto), so -m 'ipc*' ran only one module in sequential mode.
    TR_ASSERT(t, trun::caseMatch("ipcfifo",  V{"ipc*"}));
    TR_ASSERT(t, trun::caseMatch("ipcframe", V{"ipc*"}));
    TR_ASSERT(t, !trun::caseMatch("strutil", V{"ipc*"}));

    // A negated glob excludes EVERY match - the old case matcher pushed all matches then
    // assert(matches.size()==1), aborting a debug build when more than one matched.
    TR_ASSERT(t, !trun::caseMatch("split",  V{"!spl*", "-"}));
    TR_ASSERT(t, !trun::caseMatch("split2", V{"!spl*", "-"}));
    TR_ASSERT(t,  trun::caseMatch("trim",   V{"!spl*", "-"}));

    // First-match-wins: the documented "exclusions first, then '-'" idiom excludes the named
    // ones and includes the rest.
    TR_ASSERT(t, !trun::caseMatch("abortall",  V{"!abortall", "!exception", "-"}));
    TR_ASSERT(t, !trun::caseMatch("exception", V{"!abortall", "!exception", "-"}));
    TR_ASSERT(t,  trun::caseMatch("strutil",   V{"!abortall", "!exception", "-"}));

    // A plain list includes only listed names; a bare "-" includes everything.
    TR_ASSERT(t,  trun::caseMatch("timer",    V{"strutil", "timer"}));
    TR_ASSERT(t, !trun::caseMatch("rust",     V{"strutil", "timer"}));
    TR_ASSERT(t,  trun::caseMatch("anything", V{"-"}));

    // A bare positive (no "-") includes only matches; a non-match is excluded.
    TR_ASSERT(t,  trun::caseMatch("strutil", V{"strutil"}));
    TR_ASSERT(t, !trun::caseMatch("timer",   V{"strutil"}));

    return kTR_Pass;
}
