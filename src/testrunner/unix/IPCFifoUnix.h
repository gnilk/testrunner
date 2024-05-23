//
// Created by gnilk on 19.10.23.
//

#ifndef GNKLOG_LOGIPCFIFOUNIX_H
#define GNKLOG_LOGIPCFIFOUNIX_H

//#include "LogInternal.h"
#include "ipc/IPCBase.h"
namespace gnilk {
    class IPCFifoUnix : public IPCBase {
    public:
        IPCFifoUnix() = default;
        virtual ~IPCFifoUnix() = default;

        bool Open() override;
        bool ConnectTo(const std::string name);
        void Close() override;
        bool Available() override;

        const std::string &FifoName() {
            return fifoname;
        }


        int32_t Write(const void *data, size_t szBytes) override;
        int32_t Read(void *dstBuffer, size_t maxBytes) override;

    private:
        bool isOpen = false;
        bool isOwner = false;

        std::string fifoname = {};

        int fifofd = -1;
        int rwfd = -1;

    };
}


#endif //GNKLOG_LOGIPCFIFOUNIX_H
