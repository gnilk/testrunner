//
// Created by gnilk on 06.03.2026.
//

#include "Config.h"

using namespace tcov;

Config& Config::Instance() {
    static Config glbConfig;
    return glbConfig;
}
