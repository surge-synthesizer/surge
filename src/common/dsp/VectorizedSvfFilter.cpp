#include <math.h>

#include "globals.h"
#include "VectorizedSvfFilter.h"

//================================================================================================

//------------------------------------------------------------------------------------------------

VectorizedSvfFilter::VectorizedSvfFilter() { Reset(); }

//------------------------------------------------------------------------------------------------

void VectorizedSvfFilter::Reset()
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

void VectorizedSvfFilter::SetCoeff(float pOmega[4], float QVal, float Spread)
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

void VectorizedSvfFilter::CopyCoeff(const VectorizedSvfFilter &SVF)
{
    F1 = SVF.F1;
    F2 = SVF.F2;
    Q = SVF.Q;
}

//------------------------------------------------------------------------------------------------

float VectorizedSvfFilter::CalcF(float Omega) { return 2.0 * sin(M_PI * Omega); }

//------------------------------------------------------------------------------------------------

float VectorizedSvfFilter::CalcQ(float Quality) { return 1.f / Quality; }

//------------------------------------------------------------------------------------------------
