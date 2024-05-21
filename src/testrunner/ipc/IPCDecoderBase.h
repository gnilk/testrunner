//
// Created by gnilk on 21.05.24.
//

#ifndef TESTRUNNER_IPCDECODERBASE_H
#define TESTRUNNER_IPCDECODERBASE_H

#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <functional>

#include "IPCCore.h"


namespace gnilk {
    class IPCDecoderBase : public IPCReader {
    public:
        using CBOnArrayItemRead = std::function<void(IPCObject *)>;

    public:
        IPCDecoderBase() = default;
        virtual ~IPCDecoderBase() = default;

        virtual bool Process() = 0;

        // Signed
        virtual int32_t ReadI8(int8_t &outValue) = 0;
        virtual int32_t ReadI16(int16_t &outValue) = 0;
        virtual int32_t ReadI32(int32_t &outValue) = 0;
        virtual int32_t ReadI64(int64_t &outValue) = 0;

        // Unsigned
        virtual int32_t ReadU8(uint8_t &outValue) = 0;
        virtual int32_t ReadU16(uint16_t &outValue) = 0;
        virtual int32_t ReadU32(uint32_t &outValue) = 0;
        virtual int32_t ReadU64(uint64_t &outValue) = 0;

        // floating point
        virtual int32_t ReadFloat(float &outValue) = 0;
        virtual int32_t ReadDouble(double &outValue) = 0;

        // Other
        virtual int32_t ReadStr(std::string &outValue) = 0;

        virtual int32_t ReadArray(CBOnArrayItemRead onArrayItemRead) = 0;
    };
}
#endif //TESTRUNNER_IPCDECODERBASE_H
