/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#ifndef SURGE_SRC_COMMON_GLOBALS_H
#define SURGE_SRC_COMMON_GLOBALS_H

#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <algorithm>
#include <string>

// if you hit this on msvc and pass the above, you probably need /Zc:__cplusplus
static_assert(__cplusplus == 201703L, "Surge requires C++17; please update your build");

#if MAC

#if defined(__x86_64__)
#else
#define ARM_NEON 1
#endif

#endif

#if LINUX
#if defined(__aarch64__) || defined(__arm__)
#define ARM_NEON 1
#endif
#endif

#if defined(__SSE2__) || defined(_M_AMD64) || defined(_M_X64) ||                                   \
    (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
#else
// With the upgrade to simde 0.8.2 and subsequent conversations
// with simde maintainers, this include should work for every
// non-intel platform, so remove the prior condition
//
// #if defined(__arm__) || defined(__aarch64__) || defined(__riscv)
//
// and just always include this in the else side
#define SIMDE_ENABLE_NATIVE_ALIASES
#include "simde/x86/sse2.h"
#endif

#if MAC || LINUX
#include <strings.h>

static inline int _stricmp(const char *s1, const char *s2) { return strcasecmp(s1, s2); }
#endif

#define _SURGE_STR(x) #x
#define SURGE_STR(x) _SURGE_STR(x)

#if !defined(SURGE_COMPILE_BLOCK_SIZE)
#error You must compile with -DSURGE_COMPILE_BLOCK_SIZE=32 (or whatnot)
#endif

const int BASE_WINDOW_SIZE_X = 913;
const int BASE_WINDOW_SIZE_Y = 569;
const int NAMECHARS = 64;
const int BLOCK_SIZE = SURGE_COMPILE_BLOCK_SIZE;
const int OSC_OVERSAMPLING = 2;
const int BLOCK_SIZE_OS = OSC_OVERSAMPLING * BLOCK_SIZE;
const int BLOCK_SIZE_QUAD = BLOCK_SIZE >> 2;
const int BLOCK_SIZE_OS_QUAD = BLOCK_SIZE_OS >> 2;
const int OB_LENGTH = BLOCK_SIZE_OS << 1;
const int OB_LENGTH_QUAD = OB_LENGTH >> 2;
const float BLOCK_SIZE_INV = (1.f / BLOCK_SIZE);
const float BLOCK_SIZE_OS_INV = (1.f / BLOCK_SIZE_OS);
const int MAX_FB_COMB = 2048;               // must be 2^n
const int MAX_FB_COMB_EXTENDED = 2048 * 64; // Only exposed in Combulator
const int MAX_VOICES = 64;
const int MAX_UNISON = 16;
const int N_OUTPUTS = 2;
const int N_INPUTS = 2;

const int DEFAULT_POLYLIMIT = 16;

const int DEFAULT_OSC_PORT_IN = 53280;
const int DEFAULT_OSC_PORT_OUT = 53281;
const std::string DEFAULT_OSC_IPADDR_OUT = "127.0.0.1";
#endif // SURGE_SRC_COMMON_GLOBALS_H
