//
// Created by gnilk on 30.10.23.
//

#ifndef GNKLOG_LOGIPCSTREAMBASE_H
#define GNKLOG_LOGIPCSTREAMBASE_H

#include <stdint.h>
#include <string>
#include <memory>
#include "IPCCore.h"

namespace gnilk {



    class IPCBase : public IPCWriter, public IPCReader {
    public:
        using Ref = std::shared_ptr<IPCBase>;
    public:
        IPCBase() = default;
        virtual ~IPCBase() = default;

        virtual bool Open() {
            return true;
        }
        // Client side of Open(): attach to an endpoint another process already created
        // (a module subprocess reporting results back to the fork executor's server).
        virtual bool ConnectTo(const std::string name) {
            return false;
        }
        virtual void Close() {}

        // Must be overridden otherwise the system will deadlock
        // return true if there is data to be read false if no data is available
        virtual bool Available() { return false; }

        virtual int32_t Write(const void *data, size_t szBytes) { return -1; }
        virtual int32_t Read(void *dstBuffer, size_t maxBytes) { return -1; }

        // Address of the underlying transport (FIFO path / named pipe name) that a
        // child process needs in order to connect back. Override where the concrete
        // transport has one (IPCFifoUnix, IPCPipeWin); default is "no name".
        virtual const std::string &EndpointName() const {
            static const std::string empty;
            return empty;
        }

    };


}

#endif //GNKLOG_LOGIPCSTREAMBASE_H
