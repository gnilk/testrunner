//
// Created by gnilk on 06.03.2026.
//

#include <string>
#include <stdlib.h>

#include "Config.h"

#include <filesystem>

using namespace tcov;

Config& Config::Instance() {
    static Config glbConfig;
    return glbConfig;
}

static const std::string TOOL_NAME = "tcov";

bool Config::IsTrunTarget() const {
    // FIXME: this is not correct!
    return target.find("trun") != std::string::npos;
}

// made by ChatGPT - this might throw exceptions
std::string Config::ResolveCacheDir()  {
    // 1. XDG
    const char *xdg = getenv("XDG_CACHE_HOME");
    if (xdg && *xdg) {
        return (std::filesystem::path(xdg) / TOOL_NAME).string();
    }

    // 2. HOME fallback (Linux/macOS)
    const char *home = getenv("HOME");
#ifdef APPLE
    if (home && *home) {
        return (std::filesystem::path(home) / "Library/Caches/" / TOOL_NAME).string();
        //return std::string(home) + "/Library/Caches/" + TOOL_NAME;
    }
#else
    if (home && *home) {
        return (std::filesystem::path(home) / ".cache" / TOOL_NAME).string();
        //return std::string(home) + "/.cache/" + TOOL_NAME;
    }
#endif

    // 3. TMPDIR (macOS/Linux)
    const char *tmp = getenv("TMPDIR");
    if (tmp && *tmp) {
        return (std::filesystem::path(home) / TOOL_NAME).string();
//        return std::string(tmp) + "/" + TOOL_NAME;
    }

    // 4. final fallback
    return (std::filesystem::path("/tmp") / TOOL_NAME).string();
//    return std::string("/tmp/") + TOOL_NAME;
}