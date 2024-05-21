//
// Created by gnilk on 30.10.23.
//

#ifndef GNKLOG_LOGIPCSTREAMBASE_H
#define GNKLOG_LOGIPCSTREAMBASE_H

#include <stdint.h>
#include <string>

namespace gnilk {

#pragma pack(push,1)

    typedef enum : uint8_t {
        kMsgType_ResultSummary = 0x80,
    } IPCMessageType;
    typedef enum : uint8_t {
        kMsgVer_Current = 0x01,
    } IPCMessageVersion;

    struct IPCMsgHeader {
        uint8_t msgHeaderVersion = 1;
        uint8_t msgId = 0;
        uint16_t reserved = 0;
        uint32_t msgSize = 0;
    };
    struct IPCResultSummary {
        int32_t testsExecuted;
        int32_t testsFailed;
        double durationSec;
    };
    struct IPCResultTestResult {

    };
    struct IPCResultMessage {
        IPCMsgHeader header;        // Must be first in every message

        uint8_t msgVersion = 1;     // Let's add this - so I don't run into problems later on (been there done that)
        IPCResultSummary summary;
        uint32_t numResults;
//    IPCResultTestResult testResults[];  // I want this...
    };
#pragma pack(pop)

    class IPCBase {
    public:
        using Ref = std::shared_ptr<IPCBase>;
    public:
        IPCBase() = default;
        virtual ~IPCBase() = default;

        virtual bool Open() {
            return true;
        }
        virtual void Close() {}

        // Must be overridden otherwise the system will deadlock
        // return true if there is data to be read false if no data is available
        virtual bool Available() { return false; }

        virtual int32_t Write(const void *data, size_t szBytes) { return -1; }
        virtual int32_t Read(void *dstBuffer, size_t maxBytes) { return -1; }

    };

}

#endif //GNKLOG_LOGIPCSTREAMBASE_H
