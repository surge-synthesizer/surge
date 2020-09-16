#include "QuadFilterUnit.h"
#pragma once

struct QuadFilterChainState
{
   QuadFilterUnitState FU[4];

   __m128 Gain, FB, Mix1, Mix2, Drive;
   __m128 dGain, dFB, dMix1, dMix2, dDrive;

   __m128 wsLPF, FBlineL, FBlineR;

   __m128 DL[BLOCK_SIZE_OS], DR[BLOCK_SIZE_OS]; // wavedata

   __m128 OutL, OutR, dOutL, dOutR;
   __m128 Out2L, Out2R, dOut2L, dOut2R; // fb_stereo only

};

/*
** I originally had this as a member but since moved it out of line so as to
** not run any risk of alignment problems in QuadFilterChainState where
** only the head of the array is __align_malloced
*/
void InitQuadFilterChainStateToZero(QuadFilterChainState *Q);

struct fbq_global
{
   FilterUnitQFPtr FU1ptr, FU2ptr;
   WaveshaperQFPtr WSptr;
};

typedef void (*FBQFPtr)(QuadFilterChainState&, fbq_global&, float*, float*);

FBQFPtr GetFBQPointer(int config, bool A, bool WS, bool B);
