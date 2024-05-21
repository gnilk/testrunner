//
// Created by gnilk on 21.05.24.
//

#ifndef TESTRUNNER_IPCSERIALIZER_H
#define TESTRUNNER_IPCSERIALIZER_H

#include "IPCEncoderBase.h"
#include "IPCDecoderBase.h"

namespace gnilk {
    class IPCSerializer {
    public:
        IPCSerializer() = default;
        virtual ~IPCSerializer() = default;

        virtual bool Marshal(IPCEncoderBase &encoder) const = 0;
    };
    class IPCDeserializer {
    public:
        IPCDeserializer() = default;
        virtual ~IPCDeserializer() = default;

        virtual IPCDeserializer *GetDeserializerForObject(uint8_t idObject) = 0;
        virtual bool Unmarshal(IPCDecoderBase &decoder) = 0;
    };
}


#endif //TESTRUNNER_IPCSERIALIZER_H
