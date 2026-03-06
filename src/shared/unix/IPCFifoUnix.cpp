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
#include "IPCFifoUnix.h"

using namespace gnilk;

static std::string fifoBaseName= "/tmp/testrunner";

bool IPCFifoUnix::Open() {
    if (isOpen) {
        return true;
    }

    // Create name with pid - we need to be able to run multiple app's in parallel...
    auto pid = getpid();

    // if it exists - remove it - as it ought to be a left over from an old process or someone restarted the logger
    if (std::filesystem::exists(fifoname)) {
        std::filesystem::remove(fifoname);
    }

    fifoname = fifoBaseName + "_" + std::to_string(pid);

    // Create the fifo
    fifofd = mkfifo(fifoname.c_str(), 0666);
    if (fifofd < 0) {
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
        if (isOwner) {
            close(fifofd);
        }
        perror("IPCFifoUnix::Open, open");
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


    // remove the remaining fifo file - if we created it..
    if (isOwner && std::filesystem::exists(fifoname)) {
        close(fifofd);
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

    auto res =  (int32_t)write(rwfd, data, szBytes);
    if (res < 0) {
        perror("IPCFifoUnix::Write");
    }
    fsync(rwfd);
    return res;
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