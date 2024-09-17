//
// Created by Fredrik Kling on 17.08.22.
//

#ifndef TESTRUNNER_ASSERTERROR_H
#define TESTRUNNER_ASSERTERROR_H

#include <string>
#include <stdint.h>
#include <vector>

namespace trun {
    class AssertError {
    public:
        typedef enum : uint8_t {
            kAssert_Error,
            kAssert_Abort,
            kAssert_Fatal,
        } kAssertClass;

        struct AssertErrorItem {
            kAssertClass assertClass;
            int line = 0;
            std::string file;
            std::string message;
        };
    public:
        AssertError() = default;
        virtual ~AssertError() = default;

        void Add(kAssertClass aClass, int pLine, std::string pFile, std::string pMessage) {
            auto item = AssertErrorItem {
                .assertClass = aClass,
                .line = pLine,
                .file = pFile,
                .message = pMessage
            };
            assertErrors.push_back(item);
        }
        void Add(const struct AssertErrorItem &item) {
            assertErrors.push_back(item);
        }

        void Reset() {
            assertErrors.clear();
        }
        size_t NumErrors() const {
            return assertErrors.size();
        }
        const std::vector<AssertErrorItem> &Errors() const {
            return assertErrors;
        }

        bool IsValid() const {
            return !assertErrors.empty();
        }

    public:
//        bool isValid = false;
//        kAssertClass assertClass;
//        int line = 0;
//        std::string file;
//        std::string message;
    private:
        std::vector<AssertErrorItem> assertErrors;
    };
}

#endif //TESTRUNNER_ASSERTERROR_H
