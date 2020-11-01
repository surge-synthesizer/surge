/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#pragma once

#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <algorithm>

#if MAC
#include "vt_dsp/macspecific.h"

#if defined(__x86_64__)
#else
#define ARM_NEON 1
#endif

#endif

#if ARM_NEON
#define SIMDE_ENABLE_NATIVE_ALIASES
#include "simde/x86/sse2.h"
#else
#include <xmmintrin.h>

#if LINUX
#include <immintrin.h>
#endif
#endif

#if MAC || LINUX
#include <strings.h>

static inline int _stricmp(const char *s1, const char *s2)
{
   return strcasecmp(s1, s2);
}
#endif

#if WINDOWS
FILE* surge_win_fopen_utf8(const char* pathname, const char* mode);
#define fopen(pathname, mode) surge_win_fopen_utf8((pathname), (mode))
#endif

#if MAC
#define _aligned_malloc(size, alignment) malloc(size)
#define _aligned_free(memblock) free(memblock)
#elif LINUX
#define _aligned_malloc(size, alignment) aligned_alloc((alignment), (size))
#define _aligned_free(memblock) free(memblock)
#endif

#define _SURGE_STR(x) #x
#define SURGE_STR(x) _SURGE_STR(x)

const int BASE_WINDOW_SIZE_X = 904;
const int BASE_WINDOW_SIZE_Y = 569;
const int NAMECHARS = 64;
const int BLOCK_SIZE = 32;
const int OSC_OVERSAMPLING = 2;
const int BLOCK_SIZE_OS = OSC_OVERSAMPLING * BLOCK_SIZE;
const int BLOCK_SIZE_QUAD = BLOCK_SIZE >> 2;
const int BLOCK_SIZE_OS_QUAD = BLOCK_SIZE_OS >> 2;
const int OB_LENGTH = BLOCK_SIZE_OS << 1;
const int OB_LENGTH_QUAD = OB_LENGTH >> 2;
const float BLOCK_SIZE_INV = (1.f / BLOCK_SIZE);
const float BLOCK_SIZE_OS_INV = (1.f / BLOCK_SIZE_OS);
const int MAX_FB_COMB = 2048; // must be 2^n
const int MAX_VOICES = 64;
const int MAX_UNISON = 16;
const int N_OUTPUTS = 2;
const int N_INPUTS = 2;
