//
// mcu_static.h - in-house fixed-capacity containers for the zero-alloc MCU engine.
//
// Header-only, no heap, no exceptions. Just enough to back the registration tables.
//
#ifndef TRUN_MCU_STATIC_H
#define TRUN_MCU_STATIC_H

#include <stddef.h>
#include <string.h>

namespace trun {
    namespace mcu {

        // Fixed-capacity vector. PushBack returns false on overflow (deterministic,
        // local failure - never silently drops or reallocates).
        template<typename T, size_t N>
        class StaticVector {
        public:
            bool PushBack(const T &value) {
                if (count >= N) {
                    return false;
                }
                items[count] = value;
                count++;
                return true;
            }
            void Clear() {
                count = 0;
            }
            T &operator[](size_t idx) {
                return items[idx];
            }
            const T &operator[](size_t idx) const {
                return items[idx];
            }
            size_t Size() const {
                return count;
            }
            bool Full() const {
                return count >= N;
            }
            static constexpr size_t Capacity() {
                return N;
            }

        private:
            T items[N] = {};
            size_t count = 0;
        };

        // Non-owning view into a caller-owned string (typically a string literal). The
        // referenced bytes must outlive the run - true for every TRUN_ADD_TEST literal.
        struct StrView {
            const char *ptr = nullptr;
            size_t len = 0;

            StrView() = default;
            StrView(const char *_ptr, size_t _len) : ptr(_ptr), len(_len) {}

            bool Empty() const {
                return len == 0;
            }
            bool Equals(const StrView &other) const {
                if (len != other.len) {
                    return false;
                }
                return memcmp(ptr, other.ptr, len) == 0;
            }
            bool Equals(const char *str) const {
                if (str == nullptr) {
                    return false;
                }
                if (strlen(str) != len) {
                    return false;
                }
                return memcmp(ptr, str, len) == 0;
            }
        };

    } // namespace mcu
} // namespace trun

#endif // TRUN_MCU_STATIC_H
