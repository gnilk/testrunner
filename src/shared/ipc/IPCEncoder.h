//
// Created by gnilk on 21.05.24.
//

#ifndef TESTRUNNER_IPCENCODER_H
#define TESTRUNNER_IPCENCODER_H

#include <stdlib.h>
#include <string>
#include <vector>

#include "IPCCore.h"
#include "IPCEncoderBase.h"

namespace gnilk {

    class IPCBinaryEncoder : public IPCEncoderBase {
    public:
        IPCBinaryEncoder() = delete;
        explicit IPCBinaryEncoder(IPCWriter &useWriter) : writer(useWriter) {

        }
        virtual ~IPCBinaryEncoder() = default;

        bool BeginObject(uint8_t objectId) override;
        bool EndObject() override;

        bool BeginArray(uint16_t num) override;
        bool EndArray() override;


        // Signed
        __inline int32_t WriteI8(int8_t value) override { return Write(&value, sizeof(value)); }
        __inline int32_t WriteI16(int16_t value) override { return Write(&value, sizeof(value)); }
        __inline int32_t WriteI32(int32_t value) override { return Write(&value, sizeof(value)); }
        __inline int32_t WriteI64(int64_t value) override { return Write(&value, sizeof(value)); }

        // Unsigned
        __inline int32_t WriteU8(uint8_t value) override { return Write(&value, sizeof(value)); }
        __inline int32_t WriteU16(uint16_t value) override { return Write(&value, sizeof(value)); }
        __inline int32_t WriteU32(uint32_t value) override { return Write(&value, sizeof(value)); }
        __inline int32_t WriteU64(uint64_t value) override { return Write(&value, sizeof(value)); }

        // floats..
        __inline int32_t WriteFloat(float value) override { return Write(&value, sizeof(value)); }
        __inline int32_t WriteDouble(double value) override { return Write(&value, sizeof(value)); }


        // Other
        // 32-bit length prefix: a 16-bit one wrapped the length (while still writing the full
        // payload) for strings > 65535 bytes, desyncing the whole frame. Long messages / stack
        // traces / paths can exceed that. ReadStr mirrors this width.
        __inline int32_t WriteStr(const std::string &value) override {
            auto ret = WriteU32((uint32_t)value.size());
            ret += Write(value.data(), value.size());
            return ret;
        }
        __inline int32_t WriteStr(const std::string &&value) override {
            auto ret = WriteU32((uint32_t)value.size());
            ret += Write(value.data(), value.size());
            return ret;
        }

        // Routes to the currently open object's buffer, or straight to the sink
        // when no object is open. This is what lets EndObject backpatch msgSize
        // and emit each top-level message as a single contiguous write.
        int32_t Write(const void *src, size_t nBytes) override;

    protected:
        IPCWriter &writer;

    private:
        // One entry per open (nested) object; field writes append to the top.
        struct Frame {
            uint8_t objectId = 0;
            std::vector<uint8_t> body;
        };
        std::vector<Frame> stack;
    };
}


#endif //TESTRUNNER_IPCENCODER_H
