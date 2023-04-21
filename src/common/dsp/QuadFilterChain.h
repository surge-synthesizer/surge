/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2023, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#ifndef SURGE_SRC_COMMON_DSP_QUADFILTERCHAIN_H
#define SURGE_SRC_COMMON_DSP_QUADFILTERCHAIN_H

#include "globals.h"
#include "sst/filters.h"
#include "sst/waveshapers.h"

struct QuadFilterChainState
{
    sst::filters::QuadFilterUnitState FU[4];      // 2 filters left and right
    sst::waveshapers::QuadWaveshaperState WSS[2]; // 1 shaper left and right

    __m128 Gain, FB, Mix1, Mix2, Drive;
    __m128 dGain, dFB, dMix1, dMix2, dDrive;

    __m128 wsLPF, FBlineL, FBlineR;

    __m128 DL[BLOCK_SIZE_OS], DR[BLOCK_SIZE_OS]; // wavedata

    __m128 OutL, OutR, dOutL, dOutR;
    __m128 Out2L, Out2R, dOut2L, dOut2R; // fc_stereo only
};

/*
** I originally had this as a member but since moved it out of line so as to
** not run any risk of alignment problems in QuadFilterChainState where
** only the head of the array is __align_malloced
*/
void InitQuadFilterChainStateToZero(QuadFilterChainState *Q);

struct fbq_global
{
    sst::filters::FilterUnitQFPtr FU1ptr, FU2ptr;
    sst::waveshapers::QuadWaveshaperPtr WSptr;
};

typedef void (*FBQFPtr)(QuadFilterChainState &, fbq_global &, float *, float *);

FBQFPtr GetFBQPointer(int config, bool A, bool WS, bool B);

#endif // SURGE_SRC_COMMON_DSP_QUADFILTERCHAIN_H
