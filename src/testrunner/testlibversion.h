//
// Created by gnilk on 04.05.24.
//

#ifndef TESTRUNNER_TESTLIBVERSION_H
#define TESTRUNNER_TESTLIBVERSION_H

#include <stdio.h>
#include <stdint.h>
#include <string>
#include "testinterface_internal.h"

namespace trun {
// This represented the decoded version
    struct Version {
        char prefix[4]; // always 'GNK_'
        union {
            uint32_t raw;
            struct {
                uint8_t zero;
                uint8_t major;
                uint8_t minor;
                uint8_t patch;
            } ver;
        } data;

        const std::string AsString() const {
            char buffer[32];
            snprintf(buffer,31,"%d.%d.%d", data.ver.major, data.ver.minor, data.ver.patch);
            return std::string(buffer);
        }

        uint8_t Major() const {
            return data.ver.major;
        }
        uint8_t Minor() const {
            return data.ver.minor;
        }
        uint8_t Patch() const {
            return data.ver.patch;
        }
    };

    class TestLibVersion {
    public:
        static Version FromVersion(version_t rawVersion);
    };
}


#endif //TESTRUNNER_TESTLIBVERSION_H
