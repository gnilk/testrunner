//
// Created by gnilk on 05.02.2026.
//

#ifndef TESTRUNNER_COVERAGEIPCMESSAGES_H
#define TESTRUNNER_COVERAGEIPCMESSAGES_H

#include "ipc/IPCSerializer.h"

namespace trun {
    class CovIPCCmdMsg : public gnilk::IPCSerializer, public gnilk::IPCDeserializer {
    public:
        CovIPCCmdMsg() = default;
        virtual ~CovIPCCmdMsg() = default;

        bool Marshal(gnilk::IPCEncoderBase &encoder) const override;
        bool Unmarshal(gnilk::IPCDecoderBase &decoder) override;
        gnilk::IPCDeserializer *GetDeserializerForObject(uint8_t idObject) override;

        bool IsValid() {
            return isValid;
        }
        const std::string &GetSymbolName() {
            return symbolName;
        }
    protected:
        typedef enum : uint8_t {
            kMsgType_BeginSymbol = 0x80,  //
        } CovIPCMessageType;
    public:
        bool isValid = false;
        std::string symbolName = {};
    };

}

#endif //TESTRUNNER_COVERAGEIPCMESSAGES_H