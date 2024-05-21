//
// Created by gnilk on 21.05.24.
//

#ifndef TESTRUNNER_IPCDECODER_H
#define TESTRUNNER_IPCDECODER_H

#include <string>
#include <stdlib.h>
#include <stdint.h>

#include "IPCCore.h"
#include "IPCDecoderBase.h"
#include "IPCSerializer.h"

namespace gnilk {

    class IPCBinaryDecoder : public IPCDecoderBase {
    public:
        IPCBinaryDecoder(IPCReader &useReader, IPCDeserializer &useDeserializer) : reader(useReader), deserializer(useDeserializer) {

        }
        virtual ~IPCBinaryDecoder() = default;

        bool Process();

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
        int32_t ReadStr(std::string &outValue) override;
        int32_t ReadArray(CBOnArrayItemRead onArrayItemRead) override;

        int32_t Read(void *out, size_t nBytes) override {
            return reader.Read(out, nBytes);
        }
        bool Available() override {
            return reader.Available();
        }

    private:
        IPCBinaryDecoder() = default;
        IPCReader &reader;
        IPCDeserializer &deserializer;
    };


}

#endif //TESTRUNNER_IPCDECODER_H
