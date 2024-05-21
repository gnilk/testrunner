//
// Created by gnilk on 21.05.24.
//

#ifndef TESTRUNNER_IPCENCODER_H
#define TESTRUNNER_IPCENCODER_H

#include <stdlib.h>
#include <string>

#include "IPCCore.h"
#include "IPCEncoderBase.h"

namespace gnilk {

    class IPCBinaryEncoder : public IPCEncoderBase {
    public:
        IPCBinaryEncoder(IPCWriter &useWriter) : writer(useWriter) {

        }
        virtual ~IPCBinaryEncoder() = default;

        bool BeginObject(uint8_t objectId) override;
        bool EndObject() override;

        bool BeginArray(uint16_t num) override;
        bool EndArray() override;


        // Signed
        __inline int32_t WriteI8(int8_t value) { return Write(&value, sizeof(value)); }
        __inline int32_t WriteI16(int16_t value) { return Write(&value, sizeof(value)); }
        __inline int32_t WriteI32(int32_t value) { return Write(&value, sizeof(value)); }
        __inline int32_t WriteI64(int64_t value) { return Write(&value, sizeof(value)); }

        // Unsigned
        __inline int32_t WriteU8(uint8_t value) { return Write(&value, sizeof(value)); }
        __inline int32_t WriteU16(uint16_t value) { return Write(&value, sizeof(value)); }
        __inline int32_t WriteU32(uint32_t value) { return Write(&value, sizeof(value)); }
        __inline int32_t WriteU64(uint64_t value) { return Write(&value, sizeof(value)); }

        // floats..
        __inline int32_t WriteFloat(float value) { return Write(&value, sizeof(value)); }
        __inline int32_t WriteDouble(double value) { return Write(&value, sizeof(value)); }


        // Other
        __inline int32_t WriteStr(const std::string &value) {
            auto ret = WriteU16(value.size());
            ret += Write(value.data(), value.size());
            return ret;
        }
        __inline int32_t WriteStr(const std::string &&value) {
            auto ret = WriteU16(value.size());
            ret += Write(value.data(), value.size());
            return ret;
        }

        int32_t Write(const void *src, size_t nBytes) override {
            return writer.Write(src, nBytes);
        }

    private:
        IPCBinaryEncoder() = default;
    protected:
        IPCWriter &writer;

    };
}


#endif //TESTRUNNER_IPCENCODER_H
