#include "qfu.h"
#pragma once

struct fbq_store
{
   fuq_store FU[4];

   __m128 Gain, FB, Mix1, Mix2, Drive;
   __m128 dGain, dFB, dMix1, dMix2, dDrive;

   __m128 wsLPF, FBlineL, FBlineR;

   __m128 DL[block_size_os], DR[block_size_os]; // wavedata

   __m128 OutL, OutR, dOutL, dOutR;
   __m128 Out2L, Out2R, dOut2L, dOut2R; // fb_stereo only
};

struct fbq_global
{
   FilterUnitQFPtr FU1ptr, FU2ptr;
   WaveshaperQFPtr WSptr;
};

typedef void (*FBQFPtr)(fbq_store&, fbq_global&, float*, float*);

FBQFPtr GetFBQPointer(int config, bool A, bool WS, bool B);