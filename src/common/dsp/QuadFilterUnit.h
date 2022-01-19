#include "FilterCoefficientMaker.h"
#pragma once
#include "globals.h"

const int n_filter_registers = 16;
const int n_waveshaper_registers = 4;

/*
 * These are defined in QuadFilterUnit.cpp
 */
struct alignas(16) QuadFilterUnitState
{
    __m128 C[n_cm_coeffs], dC[n_cm_coeffs]; // coefficients
    __m128 R[n_filter_registers];           // registers
    float *DB[4];                           // delay buffers
    int active[4]; // 0xffffffff if voice is active, 0 if not (usable as mask)
    int WP[4];     // comb write position
};
typedef __m128 (*FilterUnitQFPtr)(QuadFilterUnitState *__restrict, __m128 in);
FilterUnitQFPtr GetQFPtrFilterUnit(int type, int subtype);

/*
 * Subtypes are integers below 16 - maybe one day go as high as 32. So we have space in the
 * int for more informatino and we mask on higher bits to allow us to
 * programmatically change features we don't expose to users, in things like
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

/*
 * These are defined in QuadFilterWaveshapers.cpp
 */
struct alignas(16) QuadFilterWaveshaperState
{
    __m128 R[n_waveshaper_registers];
    __m128 init;
};
typedef __m128 (*WaveshaperQFPtr)(QuadFilterWaveshaperState *__restrict, __m128 in, __m128 drive);
WaveshaperQFPtr GetQFPtrWaveshaper(int type);
/*
 * Given the very first sample inbound to a new voice session, return the
 * first set of registers for that voice.
 */
void initializeWaveshaperRegister(int type, float R[n_waveshaper_registers]);
