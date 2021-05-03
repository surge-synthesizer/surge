/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2021 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include "LanczosResampler.h"

float LanczosResampler::lanczosTable alignas(
    16)[LanczosResampler::tableObs + 1][LanczosResampler::filterWidth];
float LanczosResampler::lanczosTableDX alignas(
    16)[LanczosResampler::tableObs + 1][LanczosResampler::filterWidth];

bool LanczosResampler::tablesInitialized = false;

size_t LanczosResampler::populateNext(float *fL, float *fR, size_t max)
{
    int populated = 0;
    while (populated < max && (phaseI - phaseO) > A + 1)
    {
        read((phaseI - phaseO), fL[populated], fR[populated]);
        phaseO += dPhaseO;
        populated++;
    }
    return populated;
}

void LanczosResampler::populateNextBlockSizeOS(float *fL, float *fR)
{
    double r0 = phaseI - phaseO;
    for (int i = 0; i < BLOCK_SIZE_OS; ++i)
    {
        read(r0 - i * dPhaseO, fL[i], fR[i]);
    }
    phaseO += BLOCK_SIZE_OS * dPhaseO;
}