#pragma once

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
