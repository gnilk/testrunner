//
// Created by gnilk on 21.05.24.
//

#include "../IPCBufferedWriter.h"
#include "ext_testinterface/testinterface.h"
#include <string>
#include <string.h>

extern "C" {
DLL_EXPORT int test_ipcbufwriter(ITesting *t);
DLL_EXPORT int test_ipcbufwriter_write(ITesting *t);
}
DLL_EXPORT int test_ipcbufwriter(ITesting *t) {
    return kTR_Pass;
}

// Need this in order for the data to end up somewhere..
class UTest_IPC_VectorWriter : public gnilk::IPCWriter {
public:
    UTest_IPC_VectorWriter() = default;
    virtual ~UTest_IPC_VectorWriter() = default;
    int32_t Write(const void *src, size_t nBytes) override {
        auto ptrBytes = static_cast<const uint8_t *>(src);
        for(size_t i=0;i<nBytes;i++) {
            data.push_back(ptrBytes[i]);
        }
        return nBytes;
    }
    const std::vector<uint8_t> &Data() { return data; }
private:
    std::vector<uint8_t> data;
};

DLL_EXPORT int test_ipcbufwriter_write(ITesting *t) {
    UTest_IPC_VectorWriter vWriter;
    gnilk::IPCBufferedWriter bufWriter(vWriter);


    auto world = std::string("hello world");
    auto nWritten = bufWriter.Write(world.data(), world.size());
    TR_ASSERT(t, nWritten == world.size());
    printf("Left: %zu\n",bufWriter.Left());
    bufWriter.Flush();

    // No data should be available in the buffered writer...
    TR_ASSERT(t, bufWriter.Size() == 0);

    auto data = vWriter.Data();
    TR_ASSERT(t, data.size() == world.size());
    TR_ASSERT(t, memcmp (data.data(), world.data(), world.size()) == 0);


    return kTR_Pass;

}

