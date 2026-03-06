//
// Created by gnilk on 21.05.24.
//

#include "IPCBufferedWriter.h"

using namespace gnilk;


IPCBufferedWriter::IPCBufferedWriter(IPCWriter &useWriter) : writer(useWriter) {
    // Try to avoid allocations while 'writing'
    data.reserve(START_SIZE);
}

int32_t IPCBufferedWriter::Write(const void *src, size_t nBytes) {
    // FIXME: Probably a better way to do this
    auto ptrBytes = static_cast<const uint8_t *>(src);
    for(size_t i=0;i<nBytes;i++) {
        data.push_back(ptrBytes[i]);
    }
    return nBytes;
}

int32_t IPCBufferedWriter::Flush() {
    auto res = writer.Write(data.data(), data.size());
    data.clear();
    return res;
}
