#pragma once

#ifndef __aarch64__
#include <xmmintrin.h>
#include <emmintrin.h>
#if LINUX
#include <immintrin.h>
#endif
#endif

#if defined(LINUX) && defined(__aarch64__)
#include "sse2neon.h"
#endif
