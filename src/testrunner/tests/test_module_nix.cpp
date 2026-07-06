//
// Created by Fredrik Kling on 18.08.22.
//

// using dev-version, instead of install version... self-hosting..
#include <string>
#include "config.h"
#include "testinterface_internal.h"
#include "../../shared/dynlib.h"
#include "../../shared/procspawn.h"
#include "unix/dynlib_unix.h"

extern "C" {
DLL_EXPORT int test_module(ITesting *t);
DLL_EXPORT int test_module_scan(ITesting *t);
DLL_EXPORT int test_module_symbol(ITesting *t);
DLL_EXPORT int test_module_copysym(ITesting *t);
DLL_EXPORT int test_module_procoutput(ITesting *t);
}

using namespace trun;

DLL_EXPORT int test_module(ITesting *t) {
    // Since we are testing the internals we will be linking against
    // the runner configuration instance and affect the global logger etc..
    Config::Instance();
    gnilk::Logger::SetAllSinkDebugLevel(gnilk::LogLevel::kError);

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

namespace {
    class CollectStdout : public ProcessCallbackBase {
    public:
        void OnStdOutData(std::string chunk) override { data.append(chunk); }
        std::string data;
    };
}

// ConsumePipes must forward exactly the bytes read from the child, not the whole
// 1024-byte buffer. Pre-fix it null-terminated a fixed-length string and forwarded it
// with the trailing space padding (and an embedded NUL), so a 6-byte 'hello\n' arrived
// as a 1024-char blob. Drive a known-output command through Process and check the size.
DLL_EXPORT int test_module_procoutput(ITesting *t) {
    CollectStdout cb;
    Process proc("echo");           // resolved via PATH by posix_spawnp (like 'nm')
    proc.SetCallback(&cb);
    proc.AddArgument("hello");

    TR_ASSERT(t, proc.ExecuteAndWait());
    TR_ASSERT(t, cb.data == "hello\n");
    TR_ASSERT(t, cb.data.size() == 6);   // no 1024-byte padding, no embedded NUL

    return kTR_Pass;
}
