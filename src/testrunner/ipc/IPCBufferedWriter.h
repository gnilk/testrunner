//
// Created by gnilk on 21.05.24.
//

#ifndef TESTRUNNER_IPCBUFFEREDWRITER_H
#define TESTRUNNER_IPCBUFFEREDWRITER_H

#include <vector>

#include "IPCCore.h"

namespace gnilk {
    class IPCBufferedWriter : public IPCWriter{
    public:
        static constexpr size_t START_SIZE = 1024;      // should be enough
    public:
        IPCBufferedWriter(IPCWriter &useWriter);
        virtual ~IPCBufferedWriter() = default;
        int32_t Write(const void *src, size_t nBytes) override;
        size_t Size() {
            return data.size();
        }
        size_t Left() {
            return data.capacity() - data.size();
        }
        int32_t Flush();
    private:
        IPCBufferedWriter() = default;
    protected:
        IPCWriter &writer;
        std::vector<uint8_t> data;  // FIXME: I'm bloody lazy right now...
    };

}


#endif //TESTRUNNER_IPCBUFFEREDWRITER_H
