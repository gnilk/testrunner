//
// Created by gnilk on 21.05.24.
//

#ifndef TESTRUNNER_IPCENCODER_H
#define TESTRUNNER_IPCENCODER_H

#include <stdlib.h>
#include <string>

#include "IPCCore.h"

namespace gnilk {
    class IPCEncoder : public IPCWriter {
    public:
        IPCEncoder(IPCWriter &useWriter) : writer(useWriter) {

        }
        virtual ~IPCEncoder() = default;

        // Signed
        __inline int32_t WriteI8(uint8_t value) { return Write(&value, sizeof(value)); }
        __inline int32_t WriteI16(uint16_t value) { return Write(&value, sizeof(value)); }
        __inline int32_t WriteI32(uint32_t value) { return Write(&value, sizeof(value)); }
        __inline int32_t WriteI64(uint64_t value) { return Write(&value, sizeof(value)); }

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

        int32_t Write(const void *src, size_t nBytes) {
            return writer.Write(src, nBytes);
        }

    private:
        IPCEncoder() = default;
    protected:
        IPCWriter &writer;

    };
}


#endif //TESTRUNNER_IPCENCODER_H
