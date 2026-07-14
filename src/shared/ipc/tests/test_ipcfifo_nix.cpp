//
// Unit tests for IPCFifoUnix (the unix FIFO transport for fork results).
//
#include "ext_testinterface/testinterface.h"
#include "unix/IPCFifoUnix.h"

#include <string>
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <filesystem>

extern "C" {
DLL_EXPORT int test_ipcfifo(ITesting *t);
DLL_EXPORT int test_ipcfifo_removestale(ITesting *t);
DLL_EXPORT int test_ipcfifo_keepstdin(ITesting *t);
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

// Regression: Open() used to store mkfifo()'s return (0 on success) in 'fifofd', and Close() did
// close(fifofd) == close(0) - silently closing the owner's stdin. Verify an owner Open()/Close()
// leaves fd 0 exactly as it was. We point fd 0 at /dev/null first so the check is deterministic
// even when the runner was started with stdin closed/redirected, and restore the real stdin after.
DLL_EXPORT int test_ipcfifo_keepstdin(ITesting *t) {
    int savedStdin = dup(0);                          // -1 if stdin was already closed
    int devnull = open("/dev/null", O_RDONLY);
    TR_ASSERT(t, devnull >= 0);
    dup2(devnull, 0);                                 // fd 0 is now a known-open descriptor

    gnilk::IPCFifoUnix ipc;
    bool opened = ipc.Open();
    ipc.Close();

    bool stdinStillOpen = (fcntl(0, F_GETFD) != -1);  // the bug closed fd 0 in Close()

    // Restore the process's real stdin no matter what.
    if (savedStdin >= 0) {
        dup2(savedStdin, 0);
        close(savedStdin);
    }
    close(devnull);

    TR_ASSERT(t, opened);
    TR_ASSERT(t, stdinStillOpen);
    return kTR_Pass;
}
