//
// Created by gnilk on 21.05.24.
//

#ifndef TESTRUNNER_IPCMESSAGES_H
#define TESTRUNNER_IPCMESSAGES_H

#include <stdint.h>
#include <string.h>
#include <memory>
#include <utility>
#include <vector>
#include <string>

#include "testresult.h"
#include "IPCSerializer.h"

namespace gnilk {

    #pragma pack(push,1)
    typedef enum : uint8_t {
        kMsgType_ResultSummary = 0x80,  // = ModuleResults [0..N]
        kMsgType_ModuleResults = 0x81,  // = TestResults [0..N]
        kMsgType_TestResults = 0x82,
        kMsgType_AssertError = 0x83,
    } IPCMessageType;

    typedef enum : uint8_t {
        kMsgVer_Current = 0x01,
    } IPCMessageVersion;

    // And implicit message header used by encoder/decoder to determine what is coming..
    struct IPCMsgHeader {
        uint8_t msgHeaderVersion = 1;
        uint8_t msgId = 0;
        uint16_t reserved = 0;
        uint32_t msgSize = 0;
    };
    struct IPCArrayHeader {
        uint8_t headerVersion = 1;
        uint8_t reserved = 0;
        uint16_t num = 0;
    };


    // FIXME: need 'IPCAssertError'
    class IPCAssertError : public IPCSerializer, public IPCDeserializer {
    public:
        IPCAssertError() = default;
        IPCAssertError(const trun::AssertError &useAssertError) : assertError(useAssertError) {}
        virtual ~IPCAssertError() = default;

        bool Marshal(IPCEncoderBase &encoder) const override;
        bool Unmarshal(IPCDecoderBase &decoder) override;
        IPCDeserializer *GetDeserializerForObject(uint8_t idObject) override;

    public:
        class trun::AssertError assertError;
    };

    class IPCTestResults : public IPCSerializer, public IPCDeserializer {
    public:
        IPCTestResults() = default;
        IPCTestResults(const trun::TestResult::Ref &useTestResult) : testResult(useTestResult) {}
        virtual ~IPCTestResults() = default;

        bool Marshal(IPCEncoderBase &encoder) const override;
        bool Unmarshal(IPCDecoderBase &decoder) override;
        IPCDeserializer *GetDeserializerForObject(uint8_t idObject) override;
    public:
        std::string symbolName = {};
        trun::TestResult::Ref testResult = {};
        trun::AssertError assertError;      // Only valid in case we have an assert
    };

    class IPCResultSummary : public IPCSerializer, public IPCDeserializer {
    public:
        IPCResultSummary() = default;
        virtual ~IPCResultSummary() = default;

        bool Marshal(IPCEncoderBase &encoder) const override;
        bool Unmarshal(IPCDecoderBase &decoder) override;
        IPCDeserializer *GetDeserializerForObject(uint8_t idObject) override;
    public:
        int32_t testsExecuted = {};
        int32_t testsFailed = {};
        double durationSec = {};
        std::vector<IPCTestResults *> testResults;
    };




//    // Utility to allocate a block - with custom deallocation and return it as a shared ptr...
//    template<typename T>
//    std::shared_ptr<T> IPCAllocateRef(size_t nBytes) {
//        auto ptr = malloc(nBytes);
//        memset(ptr,0,nBytes);
//        // Create the memory block with a custom deleter as we are allocating more than needed
//        return std::shared_ptr<T>(static_cast<T *>(ptr), [](auto p) { free(p); });
//    }

    #pragma pack(pop)

}

#endif //TESTRUNNER_IPCMESSAGES_H
