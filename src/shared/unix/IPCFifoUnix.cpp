//
// Created by gnilk on 19.10.23.
//

//
// Event serialization using fifo's - not sure about the portability here - will have to test on macOS at least
//

#include <string>
#include <filesystem>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include "IPCFifoUnix.h"

using namespace gnilk;

static std::string fifoBaseName= "/tmp/testrunner";

bool IPCFifoUnix::Open() {
    if (isOpen) {
        return true;
    }

    // Create name with pid - we need to be able to run multiple app's in parallel...
    auto pid = getpid();
    fifoname = fifoBaseName + "_" + std::to_string(pid);

    // if it exists - remove it - as it ought to be a left over from an old process or someone restarted the logger
    if (std::filesystem::exists(fifoname)) {
        std::filesystem::remove(fifoname);
    }

    // Create the fifo. mkfifo returns 0 on success (it makes a filesystem object, not a file
    // descriptor) - the only real descriptor is rwfd, opened in ConnectTo below.
    if (mkfifo(fifoname.c_str(), 0666) < 0) {
        perror("IPCFifoUnix::Open, mkfifo");
        return false;
    }
    isOwner = true;
    return ConnectTo(fifoname);
}

bool IPCFifoUnix::ConnectTo(const std::string name) {
    fifoname = name;

    // Open it in Read/Write mode - note: this might not be supported everywhere
    rwfd = open(fifoname.c_str(), O_RDWR);
    if (rwfd < 0) {
        perror("IPCFifoUnix::ConnectTo, open");
        // If we (the owner) created the fifo in Open() but can't open it, remove it so it does
        // not leak. There is no descriptor from mkfifo to close here - the old close(fifofd) was
        // close(0), which silently closed our own stdin.
        if (isOwner && std::filesystem::exists(fifoname)) {
            std::filesystem::remove(fifoname);
        }
        return false;
    }

    // All good - tag and bag em...
    isOpen = true;
    return true;


}

void IPCFifoUnix::Close() {
    if (!isOpen) {
        return;
    }

    close(rwfd);

    // Remove the remaining fifo file - if we created it. mkfifo did not open a descriptor, so
    // there is nothing else to close here; the old close(fifofd) was close(0) -> closed stdin.
    if (isOwner && std::filesystem::exists(fifoname)) {
        std::filesystem::remove(fifoname);
    }

    isOpen = false;
}

bool IPCFifoUnix::Available() {
    struct pollfd pfd {
            .fd = rwfd,
            .events = POLLIN,
            .revents = {},
    };

    // Non-blocking poll...
    if (poll(&pfd, 1, 0)==1) {
        return true;
    }
    return false;
}


int32_t IPCFifoUnix::Write(const void *data, size_t szBytes) {
    if (!isOpen) {
        // Auto open here???
        return -1;
    }

    // Loop until everything is written. A write() to a FIFO is only guaranteed atomic up to
    // PIPE_BUF and can return a short count for a larger frame or be interrupted (EINTR);
    // the buffered writer clears its buffer after one Write, so a short write would silently
    // truncate the frame on the wire.
    auto ptr = static_cast<const uint8_t *>(data);
    size_t total = 0;
    while (total < szBytes) {
        auto res = write(rwfd, ptr + total, szBytes - total);
        if (res < 0) {
            if (errno == EINTR) {
                continue;   // interrupted before writing anything - retry
            }
            perror("IPCFifoUnix::Write");
            return -1;
        }
        total += (size_t)res;
    }
    // No fsync: rwfd is a FIFO, so fsync() is a no-op that just fails with EINVAL.
    return (int32_t)total;
}

int32_t IPCFifoUnix::Read(void *dstBuffer, size_t maxBytes) {
    if (!isOpen) {
        return -1;
    }
    if (!Available()) {
        return 0;
    }
    auto res = read(rwfd, dstBuffer, maxBytes);
    if (res < 0) {
        perror("IPCFifoUnix::Read");
    }
    return res;
}