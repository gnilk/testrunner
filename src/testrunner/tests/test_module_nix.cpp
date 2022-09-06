//
// Created by Fredrik Kling on 18.08.22.
//

// using dev-version, instead of install version... self-hosting..
#include "../testinterface.h"
#include "../dynlib.h"
#include "../dynlib_linux.h"

extern "C" {
DLL_EXPORT int test_module(ITesting *t);
DLL_EXPORT int test_module_scan(ITesting *t);
DLL_EXPORT int test_module_symbol(ITesting *t);
DLL_EXPORT int test_module_copysym(ITesting *t);
DLL_EXPORT int test_glob1(ITesting *t);
DLL_EXPORT int test_glob2(ITesting *t);
}


DLL_EXPORT int test_module(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_module_scan(ITesting *t) {
    DynLibLinux modloader;

#ifdef APPLE
    auto res = modloader.Scan("lib/libtrun_utests.dylib");
#else
    TR_ASSERT(t, modloader.Scan("lib/libtrun_utests.so"));
#endif
    TR_ASSERT(t, res);
    // We should at least find some testable stuff in our own library...
    TR_ASSERT(t, modloader.Exports().size() > 0);

    return kTR_Pass;
}

DLL_EXPORT int test_module_symbol(ITesting *t) {
    DynLibLinux modloader;

#ifdef APPLE
    auto res = modloader.Scan("lib/libtrun_utests.dylib");
#else
    TR_ASSERT(t, modloader.Scan("lib/libtrun_utests.so"));
#endif
    TR_ASSERT(t, res);
    // We should find this...
    TR_ASSERT(t, modloader.FindExportedSymbol("test_module_symbol"));
    // but not this..
    TR_ASSERT(t, !modloader.FindExportedSymbol("test_fake_symbol"));

    return kTR_Pass;
}

DLL_EXPORT int test_module_copysym(ITesting *t) {
    DynLibLinux modloader;

#ifdef APPLE
    auto res = modloader.Scan("lib/libtrun_utests.dylib");
#else
    TR_ASSERT(t, modloader.Scan("lib/libtrun_utests.so"));
#endif
    TR_ASSERT(t, res == true);
    TR_ASSERT(t, modloader.Exports().size() > 0);
    return kTR_Pass;
}
// These are global functions - but they end up as modules
DLL_EXPORT int test_glob1(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_glob2(ITesting *t) {
    return kTR_Pass;
}
