//
// Unit tests for IPCFifoUnix (the unix FIFO transport for fork results).
//
#include "ext_testinterface/testinterface.h"
#include "unix/IPCFifoUnix.h"

#include <string>
#include <cstdio>
#include <unistd.h>
#include <filesystem>

extern "C" {
DLL_EXPORT int test_ipcfifo(ITesting *t);
DLL_EXPORT int test_ipcfifo_removestale(ITesting *t);
}

DLL_EXPORT int test_ipcfifo(ITesting *t) {
    return kTR_Pass;
}

// Open() is supposed to remove a leftover fifo file from a previously crashed
// run before creating its own. The fifo path is '/tmp/testrunner_<pid>' (see
// IPCFifoUnix.cpp). Plant a stale regular file at that exact path and confirm
// Open() still succeeds (mkfifo would fail with EEXIST if it wasn't removed).
DLL_EXPORT int test_ipcfifo_removestale(ITesting *t) {
    std::string staleName = "/tmp/testrunner_" + std::to_string(getpid());

    // Start clean, then plant a stale file at the exact path Open() will use.
    std::filesystem::remove(staleName);
    FILE *f = fopen(staleName.c_str(), "w");
    TR_ASSERT(t, f != nullptr);
    fputs("stale", f);
    fclose(f);
    TR_ASSERT(t, std::filesystem::exists(staleName));

    gnilk::IPCFifoUnix ipc;
    bool opened = ipc.Open();
    ipc.Close();

    // Clean up regardless of outcome so a failed run doesn't poison the next.
    std::filesystem::remove(staleName);

    TR_ASSERT(t, opened);
    return kTR_Pass;
}
