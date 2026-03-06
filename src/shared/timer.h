#pragma once

#if defined(WIN32)
#include "platform.h"
#include <winnt.h>
#endif
#include <stdint.h>
#include <chrono>

namespace trun {

    class Timer {
    public:
        Timer();
        void Reset();
        // returns time since reset as seconds...
        double Sample();
    private:
        std::chrono::steady_clock::time_point time_start = {};
    };
}