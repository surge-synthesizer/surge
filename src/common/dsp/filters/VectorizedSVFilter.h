/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
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
#ifndef SURGE_SRC_COMMON_DSP_FILTERS_VECTORIZEDSVFILTER_H
#define SURGE_SRC_COMMON_DSP_FILTERS_VECTORIZEDSVFILTER_H

#include "globals.h"
#include <vembertech/portable_intrinsics.h>

//------------------------------------------------------------------------------------------------

class VectorizedSVFilter
{
  public:
    VectorizedSVFilter();

    void Reset();

    void SetCoeff(float Omega[4], float Q, float Spread);

    void CopyCoeff(const VectorizedSVFilter &SVF);

    inline vFloat CalcBPF(vFloat In)
    {
        L1 = vMAdd(F1, B1, L1);
        vFloat H1 = vNMSub(Q, B1, vSub(vMul(In, Q), L1));
        B1 = vMAdd(F1, H1, B1);

        L2 = vMAdd(F2, B2, L2);
        vFloat H2 = vNMSub(Q, B2, vSub(vMul(B1, Q), L2));
        B2 = vMAdd(F2, H2, B2);

        return B2;
    }

  private:
    float CalcF(float Omega);
    float CalcQ(float Quality);

    // Registers
    vFloat L1, B1, L2, B2;

    // Coefficients
    vFloat F1, F2, Q;
};

//------------------------------------------------------------------------------------------------

#endif // SURGE_SRC_COMMON_DSP_FILTERS_VECTORIZEDSVFILTER_H
