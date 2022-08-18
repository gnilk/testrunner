//
// Created by Fredrik Kling on 18.08.22.
//

// using dev-version, instead of install version... self-hosting..
#include "../testinterface.h"
#include "../module.h"
#include "../module_linux.h"

extern "C" {
DLL_EXPORT int test_module(ITesting *t);
DLL_EXPORT int test_module_scan(ITesting *t);
DLL_EXPORT int test_module_symbol(ITesting *t);
}


DLL_EXPORT int test_module(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_module_scan(ITesting *t) {
    ModuleLinux modloader;

#ifdef APPLE
    TR_ASSERT(t, modloader.Scan("lib/libtrun_utests.dylib"));
#else
    TR_ASSERT(t, modloader.Scan("lib/libtrun_utests.so"));
#endif
    // We should at least find some testable stuff in our own library...
    TR_ASSERT(t, modloader.Exports().size() > 0);

    return kTR_Pass;
}

DLL_EXPORT int test_module_symbol(ITesting *t) {
    ModuleLinux modloader;

#ifdef APPLE
    TR_ASSERT(t, modloader.Scan("lib/libtrun_utests.dylib"));
#else
    TR_ASSERT(t, modloader.Scan("lib/libtrun_utests.so"));
#endif
    // We should find this...
    TR_ASSERT(t, modloader.FindExportedSymbol("test_module_symbol"));
    // but not this..
    TR_ASSERT(t, !modloader.FindExportedSymbol("test_fake_symbol"));

    return kTR_Pass;
}
