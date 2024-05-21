//
// Created by gnilk on 21.05.24.
//

#ifndef TESTRUNNER_IPCENCODER_H
#define TESTRUNNER_IPCENCODER_H

#include <stdlib.h>
#include <string>

#include "IPCCore.h"

namespace gnilk {
    class IPCDecoderBase : public IPCReader {
    public:
        IPCDecoderBase() = default;
        virtual ~IPCDecoderBase() = default;

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
    };

    class IPCBinaryDecoder : public IPCDecoderBase {
    public:
        IPCBinaryDecoder(IPCReader &useReader) : reader(useReader) {

        }
        virtual ~IPCBinaryDecoder() = default;
        // Signed
        __inline int32_t ReadI8(int8_t &outValue) override { return Read(&outValue, sizeof(outValue)); };
        __inline int32_t ReadI16(int16_t &outValue) override { return Read(&outValue, sizeof(outValue)); };
        __inline int32_t ReadI32(int32_t &outValue) override { return Read(&outValue, sizeof(outValue)); };
        __inline int32_t ReadI64(int64_t &outValue) override { return Read(&outValue, sizeof(outValue)); };

        // Unsigned
        __inline int32_t ReadU8(uint8_t &outValue) override { return Read(&outValue, sizeof(outValue)); };
        __inline int32_t ReadU16(uint16_t &outValue) override { return Read(&outValue, sizeof(outValue)); };
        __inline int32_t ReadU32(uint32_t &outValue) override { return Read(&outValue, sizeof(outValue)); };
        __inline int32_t ReadU64(uint64_t &outValue)  override { return Read(&outValue, sizeof(outValue)); };

        // floating point
        __inline int32_t ReadFloat(float &outValue) override { return Read(&outValue, sizeof(outValue)); };
        __inline int32_t ReadDouble(double &outValue) override { return Read(&outValue, sizeof(outValue)); };

        // Other
        __inline int32_t ReadStr(std::string &outValue) override {
            uint16_t len = 0;
            ReadU16(len);
            for(int i=0;i<len;i++) {
                uint8_t ch;
                ReadU8(ch);
                outValue += (char)ch;
            }
            return 2 + len;
        }

        int32_t Read(void *out, size_t nBytes) override {
            return reader.Read(out, nBytes);
        }
        bool Available() override {
            return reader.Available();
        }

    private:
        IPCBinaryDecoder() = default;
        IPCReader &reader;
    };

    //
    //
    //
    class IPCEncoderBase : public IPCWriter {
    public:
        IPCEncoderBase() = default;
        virtual ~IPCEncoderBase() = default;

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
    class IPCBinaryEncoder : public IPCEncoderBase {
    public:
        IPCBinaryEncoder(IPCWriter &useWriter) : writer(useWriter) {

        }
        virtual ~IPCBinaryEncoder() = default;

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
