//
// Created by gnilk on 30.10.23.
//

#ifndef GNKLOG_LOGIPCSTREAMBASE_H
#define GNKLOG_LOGIPCSTREAMBASE_H

#include <stdint.h>
#include <string>

namespace gnilk {

    class IPCBase {
    public:
        using Ref = std::shared_ptr<IPCBase>;
    public:
        IPCBase() = default;
        virtual ~IPCBase() = default;

        virtual bool Open() {
            return true;
        }
        virtual void Close() {}

        // Must be overridden otherwise the system will deadlock
        // return true if there is data to be read false if no data is available
        virtual bool Available() { return false; }

        virtual int32_t Write(const void *data, size_t szBytes) { return -1; }
        virtual int32_t Read(void *dstBuffer, size_t maxBytes) { return -1; }

    };

}

#endif //GNKLOG_LOGIPCSTREAMBASE_H
