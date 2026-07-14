//
// Created by gnilk on 06.03.2026.
//

#include <string>
#include <filesystem>

#include "Config.h"

using namespace tcov;

Config& Config::Instance() {
    static Config glbConfig;
    return glbConfig;
}

bool Config::IsTrunTarget() const {
    // Explicit override wins (--trun / --no-trun).
    if (trunDetect == TrunDetect::kForceTrun) {
        return true;
    }
    if (trunDetect == TrunDetect::kForceGeneric) {
        return false;
    }
    // Auto-detect: match the executable BASENAME exactly. A substring match
    // false-positives on any path containing "trun" - a binary named 'trunembedded',
    // or any target under a '.../testrunner/...' directory - which would make tcov
    // inject --sequential/--coverage and trap SIGUSR1 for a non-trun target and then
    // hang waiting for a dylib-load signal that never comes. Use --trun to force a
    // renamed trun binary, --no-trun to force a generic binary that happens to be
    // named "trun".
    return std::filesystem::path(target).filename() == "trun";
}
