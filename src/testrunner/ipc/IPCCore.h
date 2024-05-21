//
// Created by gnilk on 21.05.24.
//

#ifndef TESTRUNNER_IPCCORE_H
#define TESTRUNNER_IPCCORE_H

#include <stdlib.h>
#include <stdint.h>

namespace gnilk {

    // Dummy - avoiding circular references..
    class IPCObject {
    public:
        IPCObject() = default;
        virtual ~IPCObject() = default;
    };

    class IPCWriter {
    public:
        virtual int32_t Write(const void *src, size_t nBytes) = 0;
    };

    class IPCReader {
    public:
        virtual int32_t Read(void *dst, size_t nBytes) = 0;
        virtual bool Available() = 0;
    };
}

#endif //TESTRUNNER_IPCCORE_H
