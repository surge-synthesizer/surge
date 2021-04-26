#pragma once
#include "SurgeStorage.h"

const int n_cm_coeffs = 8;

class FilterCoefficientMaker
{
  public:
    void MakeCoeffs(float Freq, float Reso, int Type, int SubType, SurgeStorage *storage,
                    bool tuningAdjusted);
    void Reset();
    FilterCoefficientMaker();
    float C[n_cm_coeffs], dC[n_cm_coeffs], tC[n_cm_coeffs]; // K1,K2,Q1,Q2,V1,V2,V3,etc
    void FromDirect(float N[n_cm_coeffs]);

  private:
    void ToCoupledForm(double A0inv, double A1, double A2, double B0, double B1, double B2,
                       double G);
    void ToNormalizedLattice(double A0inv, double A1, double A2, double B0, double B1, double B2,
                             double G);
    void Coeff_LP12(float Freq, float Reso, int SubType);
    void Coeff_HP12(float Freq, float Reso, int SubType);
    void Coeff_BP12(float Freq, float Reso, int SubType);
    void Coeff_Notch(float Freq, float Reso, int SubType);
    void Coeff_APF(float Freq, float Reso, int SubType);
    void Coeff_LP24(float Freq, float Reso, int SubType);
    void Coeff_HP24(float Freq, float Reso, int SubType);
    void Coeff_BP24(float Freq, float Reso, int SubType);
    void Coeff_LP4L(float Freq, float Reso, int SubType);
    void Coeff_COMB(float Freq, float Reso, int SubType);
    void Coeff_SNH(float Freq, float Reso, int SubType);
    void Coeff_SVF(float Freq, float Reso, bool);

    bool FirstRun;

    SurgeStorage *storage;
};
