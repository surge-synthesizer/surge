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
#include <math.h>

#include "globals.h"
#include "VectorizedSVFilter.h"

//================================================================================================

//------------------------------------------------------------------------------------------------

VectorizedSVFilter::VectorizedSVFilter() { Reset(); }

//------------------------------------------------------------------------------------------------

void VectorizedSVFilter::Reset()
{
    L1 = vZero;
    L2 = vZero;
    B1 = vZero;
    B2 = vZero;
    F1 = vZero;
    F2 = vZero;
    Q = vZero;
}

//------------------------------------------------------------------------------------------------

void VectorizedSVFilter::SetCoeff(float pOmega[4], float QVal, float Spread)
{
    float Freq1 alignas(16)[4];
    float Freq2 alignas(16)[4];
    float Quality alignas(16)[4];

    QVal = CalcQ(QVal);

    for (int i = 0; i < 4; i++)
    {
        Freq1[i] = CalcF(pOmega[i] * (1.f - Spread));
        Freq2[i] = CalcF(pOmega[i] * (1.f + Spread));
        Quality[i] = QVal;
    }

    F1 = vLoad(Freq1);
    F2 = vLoad(Freq2);
    Q = vLoad(Quality);
}

//------------------------------------------------------------------------------------------------

void VectorizedSVFilter::CopyCoeff(const VectorizedSVFilter &SVF)
{
    F1 = SVF.F1;
    F2 = SVF.F2;
    Q = SVF.Q;
}

//------------------------------------------------------------------------------------------------

float VectorizedSVFilter::CalcF(float Omega) { return 2.0 * sin(M_PI * Omega); }

//------------------------------------------------------------------------------------------------

float VectorizedSVFilter::CalcQ(float Quality) { return 1.f / Quality; }

//------------------------------------------------------------------------------------------------
