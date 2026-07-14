//
// tcov_utests - SymbolResolver / SymbolInfo type tests.
//
// These pin §2's static-data decisions from todo/tcov_cleanup.md, written BEFORE
// the Phase 2 refactor:
//
//   D1 - SymbolInfo is the single source of truth for a symbol's STATIC data and
//        carries a startLoadAddress AND endLoadAddress (named to match struct
//        Function, their origin); both default to 0.
//
// ResolveForTarget itself needs a live lldb::SBTarget, so it stays covered by the
// integration before/after coverage diff (see the plan's §0 boundary note), not
// here - these cases exercise the tcov *types* only.
//
#include "ext_testinterface/testinterface.h"
#include "SymbolResolver.h"

using namespace tcov;

extern "C" {
    DLL_EXPORT int test_symbolresolver(ITesting *t);
    DLL_EXPORT int test_symbolresolver_exit(ITesting *t);
    DLL_EXPORT int test_symbolresolver_infodefaults(ITesting *t);
    DLL_EXPORT int test_symbolresolver_infoendaddr(ITesting *t);
}

// Module main / exit - no shared state needed yet.
DLL_EXPORT int test_symbolresolver(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_symbolresolver_exit(ITesting *t) {
    return kTR_Pass;
}

// D1: a freshly-constructed SymbolInfo has empty strings and zeroed addresses,
//     including the new endLoadAddress field.
DLL_EXPORT int test_symbolresolver_infodefaults(ITesting *t) {
    SymbolResolver::SymbolInfo info;
    TR_ASSERT(t, info.name.empty());
    TR_ASSERT(t, info.full.empty());
    TR_ASSERT(t, info.file.empty());
    TR_ASSERT(t, info.line == 0);
    TR_ASSERT(t, info.startLoadAddress == 0);
    TR_ASSERT(t, info.endLoadAddress == 0);
    return kTR_Pass;
}

// D1: start and end load addresses are independent fields describing a range.
DLL_EXPORT int test_symbolresolver_infoendaddr(ITesting *t) {
    SymbolResolver::SymbolInfo info;
    info.startLoadAddress = 0x1000;
    info.endLoadAddress = 0x1040;
    TR_ASSERT(t, info.startLoadAddress == 0x1000);
    TR_ASSERT(t, info.endLoadAddress == 0x1040);
    TR_ASSERT(t, info.endLoadAddress > info.startLoadAddress);
    return kTR_Pass;
}
