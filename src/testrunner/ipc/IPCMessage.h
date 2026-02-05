//
// Created by gnilk on 05.02.2026.
//

#ifndef TESTRUNNER_IPCMESSAGE_H
#define TESTRUNNER_IPCMESSAGE_H

#include <stdint.h>

namespace gnilk {
#pragma pack(push,1)

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
#pragma pack(pop)

}

#endif //TESTRUNNER_IPCMESSAGE_H