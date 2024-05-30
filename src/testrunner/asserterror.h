//
// Created by Fredrik Kling on 17.08.22.
//

#ifndef TESTRUNNER_ASSERTERROR_H
#define TESTRUNNER_ASSERTERROR_H

#include <string>
#include <stdint.h>

namespace trun {
    class AssertError {
    public:
        typedef enum : uint8_t {
            kAssert_Error,
            kAssert_Abort,
            kAssert_Fatal,
        } kAssertClass;

        AssertError() = default;
        virtual ~AssertError() = default;

        void Set(kAssertClass aClass, int pLine, std::string pFile, std::string pMessage) {
            isValid = true;
            assertClass = aClass;
            line = pLine;
            file = pFile;
            message = pMessage;
        }

        void Reset() {
            isValid = false;
        }

    public:
        bool isValid = false;
        kAssertClass assertClass;
        int line = 0;
        std::string file;
        std::string message;
    };
}

#endif //TESTRUNNER_ASSERTERROR_H
