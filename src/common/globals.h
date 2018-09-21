//-------------------------------------------------------------------------------------------------------
//	Copyright 2005-2006 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <algorithm>

#if MAC
#include <vt_dsp/macspecific.h>
#endif
#if !PPC
#include <xmmintrin.h>
#endif

using namespace std;

const int namechars = 64;
const int block_size = 32;
const int osc_oversampling = 2;
const int block_size_os = osc_oversampling * block_size;
const int block_size_quad = block_size >> 2;
const int block_size_os_quad = block_size_os >> 2;
const int max_unison = 16;
const int ob_length = block_size_os << 1;
const int ob_length_quad = ob_length >> 2;
const float block_size_inv = (1.f / block_size);
const float block_size_os_inv = (1.f / block_size_os);
const int max_fb_comb = 2048; // måste vara 2^n
#if !MAC
extern int SSE_VERSION;
#endif