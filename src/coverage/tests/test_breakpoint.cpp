//
// tcov_utests - BreakpointManager / Function / CompileUnit type tests.
//
// These pin §2's decisions from todo/tcov_cleanup.md, written BEFORE the Phase 2
// refactor:
//
//   D5 - report identity: CompileUnit::functions is keyed by the full (with-args)
//        display name, so overloaded functions stay distinct entries.
//
// NOTE on D3/D4 (Function composes SymbolInfo; GetDisplayName() == info.name):
// those cannot be expressed as clean red-before/green-after cases here - today
// Function::GetDisplayName() reads its lldb::SBSymbol member, and a default SBSymbol
// yields a null display name (a crash, not an assert). Function does not gain its
// `SymbolInfo info` member until the Phase 2 recomposition that also flips
// GetDisplayName() and populates it. The D3/D4 cases therefore land WITH Phase 2,
// alongside the change they guard. See the plan's §0 note.
//
#include "ext_testinterface/testinterface.h"
#include "Breakpoint.h"

using namespace tcov;

extern "C" {
    DLL_EXPORT int test_breakpoint(ITesting *t);
    DLL_EXPORT int test_breakpoint_exit(ITesting *t);
    DLL_EXPORT int test_breakpoint_cu_addfunction(ITesting *t);
    DLL_EXPORT int test_breakpoint_cu_overloadsdistinct(ITesting *t);
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
    TR_ASSERT(t, bar->name == "Foo::Bar(int)");

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
