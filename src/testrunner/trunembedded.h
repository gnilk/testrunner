//
// Created by gnilk on 11/21/2022.
//
// DEPRECATED compatibility shim.
// The desktop-embedded facade moved to <trunlib.h> (engine source: trunlib.cpp). This header
// only forwards to it and will be REMOVED in v5.0.0. Update your includes:
//     #include <trunembedded.h>   ->   #include <trunlib.h>
//
#ifndef TESTRUNNER_TRUNEMBEDDED_H
#define TESTRUNNER_TRUNEMBEDDED_H

// #pragma message (not #warning): portable across GCC/Clang/MSVC and informational, so it
// nudges without breaking consumers that build with -Werror.
#pragma message("trunembedded.h is deprecated and will be removed in v5.0.0 - include <trunlib.h> instead")

#include "trunlib.h"

#endif //TESTRUNNER_TRUNEMBEDDED_H
