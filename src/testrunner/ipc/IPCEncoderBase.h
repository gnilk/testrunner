//
// Created by gnilk on 21.05.24.
//

#ifndef TESTRUNNER_IPCENCODERBASE_H
#define TESTRUNNER_IPCENCODERBASE_H

#include <stdlib.h>
#include <stdint.h>
#include <string>

#include "IPCCore.h"

namespace gnilk {
    //
    //
    //
    class IPCEncoderBase : public IPCWriter {
    public:
        IPCEncoderBase() = default;
        virtual ~IPCEncoderBase() = default;
        // Objects
        virtual bool BeginObject(uint8_t objectId) = 0;
        virtual bool EndObject() = 0;

        virtual bool BeginArray(uint16_t num) = 0;
        virtual bool EndArray() = 0;

        // Signed
        virtual int32_t WriteI8(int8_t value) = 0;
        virtual int32_t WriteI16(int16_t value) = 0;
        virtual int32_t WriteI32(int32_t value) = 0;
        virtual int32_t WriteI64(int64_t value) = 0;

        // Unsigned
        virtual int32_t WriteU8(uint8_t value) = 0;
        virtual int32_t WriteU16(uint16_t value) = 0;
        virtual int32_t WriteU32(uint32_t value) = 0;
        virtual int32_t WriteU64(uint64_t value) = 0;

        // floats..
        virtual int32_t WriteFloat(float value) = 0;
        virtual int32_t WriteDouble(double value) = 0;

        // Other
        virtual int32_t WriteStr(const std::string &value) = 0;
        virtual int32_t WriteStr(const std::string &&value) = 0;
    };

}

#endif //TESTRUNNER_IPCENCODERBASE_H
