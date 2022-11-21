//
// Created by Fredrik Kling on 18.08.22.
//

// using dev-version, instead of install version... self-hosting..
#include "../config.h"
#include "../testinterface.h"
#include "../dynlib.h"
#include "../unix/dynlib_unix.h"

extern "C" {
DLL_EXPORT int test_module(ITesting *t);
DLL_EXPORT int test_module_scan(ITesting *t);
DLL_EXPORT int test_module_symbol(ITesting *t);
DLL_EXPORT int test_module_copysym(ITesting *t);
}

using namespace trun;

DLL_EXPORT int test_module(ITesting *t) {
    // Since we are testing the internals we will be linking against
    // the runner configuration instance and affect the global logger etc..
    Config::Instance();
    Logger::SetAllSinkDebugLevel(Logger::kMCError);

    return kTR_Pass;
}

DLL_EXPORT int test_module_scan(ITesting *t) {
    DynLibLinux modloader;

#ifdef APPLE
    auto res = modloader.Scan("lib/libtrun_utests.dylib");
#else
    auto res = modloader.Scan("lib/libtrun_utests.so");
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
    auto res = modloader.Scan("lib/libtrun_utests.so");
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
    auto res = modloader.Scan("lib/libtrun_utests.so");
#endif
    TR_ASSERT(t, res == true);
    TR_ASSERT(t, modloader.Exports().size() > 0);
    return kTR_Pass;
}
