#pragma once
#include "globals.h"

const int n_waveshaper_registers = 4;

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
