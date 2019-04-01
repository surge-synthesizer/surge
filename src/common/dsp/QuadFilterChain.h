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

   void initToZero()
   {
       Gain = _mm_setzero_ps();
       FB = _mm_setzero_ps();
       Mix1 = _mm_setzero_ps();
       Mix2 = _mm_setzero_ps();
       Drive = _mm_setzero_ps();
       dGain = _mm_setzero_ps();
       dFB = _mm_setzero_ps();
       dMix1 = _mm_setzero_ps();
       dMix2 = _mm_setzero_ps();
       dDrive = _mm_setzero_ps();

       wsLPF = _mm_setzero_ps();
       FBlineL = _mm_setzero_ps();
       FBlineR = _mm_setzero_ps();

       for(auto i=0; i<BLOCK_SIZE_OS; ++i)
       {
           DL[i] = _mm_setzero_ps();
           DR[i] = _mm_setzero_ps();
       }

       OutL = _mm_setzero_ps();
       OutR = _mm_setzero_ps();
       dOutL = _mm_setzero_ps();
       dOutR = _mm_setzero_ps();
       Out2L = _mm_setzero_ps();
       Out2R = _mm_setzero_ps();
       dOut2L = _mm_setzero_ps();
       dOut2R = _mm_setzero_ps();
   }
};

struct fbq_global
{
   FilterUnitQFPtr FU1ptr, FU2ptr;
   WaveshaperQFPtr WSptr;
};

typedef void (*FBQFPtr)(QuadFilterChainState&, fbq_global&, float*, float*);

FBQFPtr GetFBQPointer(int config, bool A, bool WS, bool B);
