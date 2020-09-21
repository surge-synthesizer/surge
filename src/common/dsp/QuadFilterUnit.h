#include "FilterCoefficientMaker.h"
#pragma once
#include "globals.h"


const int n_filter_registers = 16;

struct QuadFilterUnitState
{
   __m128 C[n_cm_coeffs], dC[n_cm_coeffs]; // coefficients
   __m128 R[n_filter_registers];           // registers
   float* DB[4];                           // delay buffers
   int active[4]; // 0xffffffff if voice is active, 0 if not (usable as mask)
   int WP[4];     // comb write position
};

typedef __m128 (*FilterUnitQFPtr)(QuadFilterUnitState* __restrict, __m128 in);
typedef __m128 (*WaveshaperQFPtr)(__m128 in, __m128 drive);

FilterUnitQFPtr GetQFPtrFilterUnit(int type, int subtype);
WaveshaperQFPtr GetQFPtrWaveshaper(int type);
