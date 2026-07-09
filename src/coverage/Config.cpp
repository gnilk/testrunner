//
// Created by gnilk on 06.03.2026.
//

#include <string>

#include "Config.h"

using namespace tcov;

Config& Config::Instance() {
    static Config glbConfig;
    return glbConfig;
}

bool Config::IsTrunTarget() const {
    // FIXME: this is not correct!
    return target.find("trun") != std::string::npos;
}
