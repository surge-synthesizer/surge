#include "vec_svf.h"

//================================================================================================

//------------------------------------------------------------------------------------------------

VecSVF::VecSVF()
{
   Reset();
}

//------------------------------------------------------------------------------------------------

void VecSVF::Reset()
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

void VecSVF::SetCoeff(float pOmega[4], float QVal, float Spread)
{
   vAlign float Freq1[4];
   vAlign float Freq2[4];
   vAlign float Quality[4];

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

void VecSVF::CopyCoeff(const VecSVF& SVF)
{
   F1 = SVF.F1;
   F2 = SVF.F2;
   Q = SVF.Q;
}

//------------------------------------------------------------------------------------------------

float VecSVF::CalcF(float Omega)
{
   return 2.0 * sin(M_PI * Omega);
}

//------------------------------------------------------------------------------------------------

float VecSVF::CalcQ(float Quality)
{
   return 1.f / Quality;
}

//------------------------------------------------------------------------------------------------