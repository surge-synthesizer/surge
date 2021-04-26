#include "FilterCoefficientMaker.h"
#pragma once
#include "globals.h"

const int n_filter_registers = 16;

struct QuadFilterUnitState
{
    __m128 C[n_cm_coeffs], dC[n_cm_coeffs]; // coefficients
    __m128 R[n_filter_registers];           // registers
    float *DB[4];                           // delay buffers
    int active[4]; // 0xffffffff if voice is active, 0 if not (usable as mask)
    int WP[4];     // comb write position
};

typedef __m128 (*FilterUnitQFPtr)(QuadFilterUnitState *__restrict, __m128 in);
typedef __m128 (*WaveshaperQFPtr)(__m128 in, __m128 drive);

FilterUnitQFPtr GetQFPtrFilterUnit(int type, int subtype);
WaveshaperQFPtr GetQFPtrWaveshaper(int type);

/*
 * Subtypes are integers below 16 - maybe one day go as high as 32. So we have space in the
 * int for more informatino and we mask on higher bits to allow us to
 * programatically change features we don't expose to users, in things like
 * FX. So far this is only used to extend the COMB time in the combulator.
 *
 * These should obvioulsy be distinct per type but can overlap in values otherwise
 *
 * Try and use above 2^16
 */
enum QFUSubtypeMasks : int32_t
{
    UNMASK_SUBTYPE = (1 << 8) - 1,
    EXTENDED_COMB = 1 << 9
};