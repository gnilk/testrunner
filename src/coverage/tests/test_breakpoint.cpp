//
// tcov_utests - BreakpointManager / Function / CompileUnit type tests.
//
// These pin §2's decisions from todo/tcov_cleanup.md:
//
//   D3/D4 - Function composes SymbolInfo; GetDisplayName() returns info.name (the
//           normalized, no-args name) and no longer reads an lldb::SBSymbol member,
//           so a default-constructed Function is safe (previously a crash).
//   D5    - report identity: CompileUnit::functions is keyed by the full (with-args)
//           display name, so overloaded functions stay distinct entries.
//
// D3/D4 landed WITH the Phase 2 recomposition that added `SymbolInfo info` to Function
// and flipped GetDisplayName() - they could not be a clean red-before here because the
// old GetDisplayName() dereferenced a default SBSymbol (a crash, not an assert).
//
#include "ext_testinterface/testinterface.h"
#include "Breakpoint.h"

using namespace tcov;

extern "C" {
    DLL_EXPORT int test_breakpoint(ITesting *t);
    DLL_EXPORT int test_breakpoint_exit(ITesting *t);
    DLL_EXPORT int test_breakpoint_cu_addfunction(ITesting *t);
    DLL_EXPORT int test_breakpoint_cu_overloadsdistinct(ITesting *t);
    DLL_EXPORT int test_breakpoint_func_composesinfo(ITesting *t);
    DLL_EXPORT int test_breakpoint_func_displaynamedefault(ITesting *t);
}

// Module main / exit - no shared state needed yet.
DLL_EXPORT int test_breakpoint(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_breakpoint_exit(ITesting *t) {
    return kTR_Pass;
}

// GetOrAddFunction returns the SAME Function for a repeated key and a NEW one for a
// fresh key; the created Function's name mirrors the key.
DLL_EXPORT int test_breakpoint_cu_addfunction(ITesting *t) {
    CompileUnit cu;

    auto bar = cu.GetOrAddFunction(std::string("Foo::Bar(int)"));
    TR_ASSERT(t, bar != nullptr);
    TR_ASSERT(t, cu.functions.size() == 1);
    TR_ASSERT(t, bar->info.full == "Foo::Bar(int)");   // identity is the with-args map key

    auto again = cu.GetOrAddFunction(std::string("Foo::Bar(int)"));
    TR_ASSERT(t, again == bar);            // same key -> same object
    TR_ASSERT(t, cu.functions.size() == 1);

    return kTR_Pass;
}

// D5: two overloads share a normalized name but differ by signature - keying the
// map by the full (with-args) display name keeps them as distinct functions.
DLL_EXPORT int test_breakpoint_cu_overloadsdistinct(ITesting *t) {
    CompileUnit cu;

    auto barInt = cu.GetOrAddFunction(std::string("Foo::Bar(int)"));
    auto barFloat = cu.GetOrAddFunction(std::string("Foo::Bar(float)"));

    TR_ASSERT(t, barInt != barFloat);
    TR_ASSERT(t, cu.functions.size() == 2);

    return kTR_Pass;
}

// D3/D4: Function composes SymbolInfo. GetDisplayName() returns the composed info.name
// (the normalized, no-args name used by the LCOV/console reports) - not info.full.
DLL_EXPORT int test_breakpoint_func_composesinfo(ITesting *t) {
    Function fn;
    fn.info.name = "Foo::Bar";            // normalized (no args) - report display
    fn.info.full = "Foo::Bar(int)";       // with args - map key / ReportDiff identity

    TR_ASSERT(t, fn.GetDisplayName() == fn.info.name);
    TR_ASSERT(t, fn.GetDisplayName() == "Foo::Bar");
    TR_ASSERT(t, fn.GetDisplayName() != fn.info.full);

    return kTR_Pass;
}

// D3/D4: a default-constructed Function has an empty display name and does NOT crash.
// Before the recomposition GetDisplayName() dereferenced a default lldb::SBSymbol whose
// display name is null - this case would have crashed rather than returned empty.
DLL_EXPORT int test_breakpoint_func_displaynamedefault(ITesting *t) {
    Function fn;
    TR_ASSERT(t, fn.GetDisplayName().empty());
    return kTR_Pass;
}
