//
// Created by gnilk on 21.05.24.
//

#ifndef TESTRUNNER_TEST_IPC_COMMON_H
#define TESTRUNNER_TEST_IPC_COMMON_H

#include <vector>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "../IPCCore.h"

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

class UTest_IPC_VectorReader : public gnilk::IPCReader {
public:
    UTest_IPC_VectorReader(const std::vector<uint8_t> &useData) : data(useData) {}
    virtual ~UTest_IPC_VectorReader() = default;
    int32_t Read(void *out, size_t nBytes) override {
        auto ptrBytes = static_cast<uint8_t *>(out);
        for(size_t i=0;i<nBytes;i++) {
            assert(idx < data.size());
            ptrBytes[i] = data[idx++];
        }
        return nBytes;
    }
    bool Available() override {
        return (data.size() > idx);
    }
    const std::vector<uint8_t> &Data() { return data; }
private:
    size_t idx = 0;
    const std::vector<uint8_t> &data;
};

#endif //TESTRUNNER_TEST_IPC_COMMON_H
