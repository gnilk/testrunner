//
// Created by gnilk on 21.05.24.
//

#ifndef TESTRUNNER_IPCMESSAGES_H
#define TESTRUNNER_IPCMESSAGES_H

#include <stdint.h>
#include <string.h>
#include <memory>
#include <utility>

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

    // Utility to allocate a block - with custom deallocation and return it as a shared ptr...
    template<typename T>
    std::shared_ptr<T> IPCAllocateRef(size_t nBytes) {
        auto ptr = malloc(nBytes);
        memset(ptr,0,nBytes);
        // Create the memory block with a custom deleter as we are allocating more than needed
        return std::shared_ptr<T>(static_cast<T *>(ptr), [](auto p) { free(p); });
    }

    struct IPCString {
        using Ref = std::shared_ptr<IPCString>;
        static Ref Create(const std::string &str) {
            size_t nBytes = sizeof(IPCString::len) + str.size() + 1;    // need terminating zero..
            auto instance = IPCAllocateRef<IPCString>(nBytes);

            // Now, assign..
            instance->len = str.length();
            memcpy(instance->data, str.data(), str.size());
            return instance;
        }
        std::string_view String() {
            return {(char *)data};
        }

        // The actual data members...
        uint16_t len;       // 64k for module names should be enough for everyone
        uint8_t data[];     // toss a warning for your witcher...
    };

    struct IPCResultMessage {
        uint8_t msgVersion = 1;     // Let's add this - so I don't run into problems later on (been there done that)
        IPCResultSummary summary;
        uint32_t numResults;
        IPCString moduleName;
        //IPCResultTestResult testResults[];  // I want this...
    };
    #pragma pack(pop)

}

#endif //TESTRUNNER_IPCMESSAGES_H
