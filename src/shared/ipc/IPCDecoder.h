//
// Created by gnilk on 21.05.24.
//

#ifndef TESTRUNNER_IPCDECODER_H
#define TESTRUNNER_IPCDECODER_H

#include <string>
#include <stdlib.h>
#include <stdint.h>
#include <vector>

#include "IPCCore.h"
#include "IPCDecoderBase.h"
#include "IPCSerializer.h"

namespace gnilk {

    class IPCBinaryDecoder : public IPCDecoderBase {
    public:
        IPCBinaryDecoder() = delete;
        IPCBinaryDecoder(IPCReader &useReader, IPCDeserializer &useDeserializer) : reader(useReader), deserializer(useDeserializer) {

        }
        virtual ~IPCBinaryDecoder() = default;

        bool Process() override;

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
        IPCObject *ReadObject(uint8_t expectedMsgId) override;

        // Sources from the in-memory frame while a message is being parsed,
        // otherwise loops-to-fill straight from the transport.
        int32_t Read(void *out, size_t nBytes) override;
        bool Available() override {
            return reader.Available();
        }

    private:
        // Loop until nBytes are read from the transport (handles short/partial
        // reads). Returns the number of bytes actually read: nBytes on success,
        // less on EOF/would-block, -1 on a hard error.
        int32_t ReadFromTransport(void *out, size_t nBytes);

    private:
        IPCReader &reader;
        IPCDeserializer &deserializer;

        // The whole current message body, pulled in by Process(); field reads
        // during Unmarshal are served from here.
        std::vector<uint8_t> frameBuf;
        size_t frameOffset = 0;
        bool frameActive = false;
    };


}

#endif //TESTRUNNER_IPCDECODER_H
