#include "FilterCoefficientMaker.h"
#include "SurgeStorage.h"
#include <vt_dsp/basic_dsp.h>

using namespace std;

const float smooth = 0.2f;

FilterCoefficientMaker::FilterCoefficientMaker()
{
   Reset();
}

void FilterCoefficientMaker::MakeCoeffs(
    float Freq, float Reso, int Type, int SubType, SurgeStorage* storageI)
{
   storage = storageI;
   switch (Type)
   {
   case fut_lp12:
      if (SubType == st_SVF)
         Coeff_SVF(Freq, Reso, false);
      else
         Coeff_LP12(Freq, Reso, SubType);
      break;
   case fut_hp12:
      if (SubType == st_SVF)
         Coeff_SVF(Freq, Reso, false);
      else
         Coeff_HP12(Freq, Reso, SubType);
      break;
   case fut_bp12:
      if ((SubType == st_SVF) || (SubType == 3))
         Coeff_SVF(Freq, Reso, false);
      else
         Coeff_BP12(Freq, Reso, SubType);
      break;
   case fut_br12:
      Coeff_BR12(Freq, Reso, SubType);
      break;
   case fut_lp24:
      if (SubType == st_SVF)
         Coeff_SVF(Freq, Reso, true);
      else
         Coeff_LP24(Freq, Reso, SubType);
      break;
   case fut_hp24:
      if (SubType == st_SVF)
         Coeff_SVF(Freq, Reso, true);
      else
         Coeff_HP24(Freq, Reso, SubType);
      break;
   case fut_lpmoog:
      Coeff_LP4L(Freq, Reso, SubType);
      break;
   case fut_comb:
      Coeff_COMB(Freq, Reso, SubType);
      break;
   case fut_SNH:
      Coeff_SNH(Freq, Reso, SubType);
      break;
   };
}

float clipscale(float freq, int subtype)
{
   switch (subtype)
   {
   case st_Rough:
      return (1.0f / 64.0f) * db_to_linear(freq * 0.55f);
   case st_Smooth:
      return (1.0f / 1024.0f); // * db_to_linear(freq*0.55f);
   };
   return 0;
}

#define boundfreq(freq)                                                                            \
   {                                                                                               \
      if (freq > 75)                                                                               \
         freq = 75;                                                                                \
      if (freq < -55)                                                                              \
         freq = -55;                                                                               \
   }

double Map2PoleResonance(double reso, double freq, int subtype)
{
   switch (subtype)
   {
   case st_Medium:
      reso *= max(0.0, 1.0 - max(0.0, (freq - 58) * 0.05));
      return (0.99 - 1.0 * limit_range((double)(1 - (1 - reso) * (1 - reso)), 0.0, 1.0));
   case st_Rough:
      reso *= max(0.0, 1.0 - max(0.0, (freq - 58) * 0.05));
      return (1.0 - 1.05 * limit_range((double)(1 - (1 - reso) * (1 - reso)), 0.001, 1.0));
   default:
   case st_Smooth:
      return (2.5 - 2.45 * limit_range((double)(1 - (1 - reso) * (1 - reso)), 0.0, 1.0));
   }
   return 0;
}

double Map2PoleResonance_noboost(double reso, double freq, int subtype)
{
   if (subtype == st_Rough)
      return (1.0 - 0.99 * limit_range((double)(1 - (1 - reso) * (1 - reso)), 0.001, 1.0));
   else
      return (0.99 - 0.98 * limit_range((double)(1 - (1 - reso) * (1 - reso)), 0.0, 1.0));
}

double Map4PoleResonance(double reso, double freq, int subtype)
{
   switch (subtype)
   {
   case st_Medium:
      reso *= max(0.0, 1.0 - max(0.0, (freq - 58) * 0.05));
      return 0.99 - 0.9949 * limit_range((double)reso, 0.0, 1.0); // sqrt(1.01) = 1.004987562
   case st_Rough:
      reso *= max(0.0, 1.0 - max(0.0, (freq - 58) * 0.05));
      return (1.0 - 1.05 * limit_range((double)reso, 0.001, 1.0));
   default:
   case st_Smooth:
      return (2.5 - 2.3 * limit_range((double)reso, 0.0, 1.0));
   }
}

double resoscale(double reso, int subtype)
{
   switch (subtype)
   {
   case st_Medium:
      return (1.0 - 0.75 * reso * reso);
   case st_Rough:
      return (1.0 - 0.5 * reso * reso);
   case st_Smooth:
      return (1.0 - 0.25 * reso * reso);
   }

   return 1.0;
}

double resoscale4Pole(double reso, int subtype)
{
   switch (subtype)
   {
   case st_Medium:
      return (1.0 - 0.75 * reso);
   case st_Rough:
      return (1.0 - 0.5 * reso * reso);
   case st_Smooth:
      return (1.0 - 0.5 * reso);
   }

   return 1.0;
}

void FilterCoefficientMaker::Coeff_SVF(float Freq, float Reso, bool FourPole)
{
   double f = 440.f * storage->note_to_pitch_ignoring_tuning(Freq);
   double F1 = 2.0 * sin(M_PI * min(0.11, f * (0.25 * samplerate_inv))); // 4x oversampling
   // double Q1 = 2.0 - Reso*2.0;

   Reso = sqrt(limit_range(Reso, 0.f, 1.f));

   // double Q1 = min(2.0*(1.0 - pow(Reso, 0.25f)), min(2.0, 2.0/F1 - F1*0.5));
   double overshoot = FourPole ? 0.1 : 0.15;
   double Q1 = 2.0 - Reso * (2.0 + overshoot) + F1 * F1 * overshoot * 0.9;
   // Q1 = min(Q1, min(2.00, 1.99/F1 - F1*0.51));
   Q1 = min(Q1, min(2.00, 2.00 - 1.52 * F1));

   double ClipDamp = 0.1 * Reso * F1;

   const double a = 0.65;
   // double Gain = a + Q1 * 0.5 * (1-a);
   double Gain = 1 - a * Reso;

   float c[n_cm_coeffs];
   memset(c, 0, sizeof(float) * n_cm_coeffs);
   c[0] = F1;
   c[1] = Q1;
   c[2] = ClipDamp;
   c[3] = Gain;
   FromDirect(c);
}

void FilterCoefficientMaker::Coeff_LP12(float freq, float reso, int subtype)
{
   float gain = resoscale(reso, subtype);

   boundfreq(freq)

       float cosi,
       sinu;
   storage->note_to_omega_ignoring_tuning(freq, sinu, cosi);

   double alpha = sinu * Map2PoleResonance(reso, freq, subtype);
   if (subtype != st_Smooth)
      alpha = min(alpha, sqrt(1.0 - cosi * cosi) - 0.0001);

   double b0 = (1 - cosi) * 0.5, b1 = 1 - cosi, b2 = (1 - cosi) * 0.5, a0 = 1 + alpha,
          a1 = -2 * cosi, a2 = 1 - alpha, a0inv = 1 / a0;

   // double sq = a1*a1 - 4.0*a2;
   // must be negative
   // dvs a2 > 0.25*a1*a1
   // 1-alpha / (1+alpha) > 0.25*(-2*cosi/(1+alpha))^2
   // 1-alpha > 0.25*(-2*cosi)^2 / (1+alpha)
   // (1-alpha)(1+alpha) > 0.25*(-2*cosi)^2
   // (1-alpha)(1+alpha) > (cosi)^2
   // >> alpha = +- sqrt(1 - cosi^2)

   // ar = 0.5 * -a1;
   // ai = 0.5 * sqrt(-sq);
   // ar = -(K*K - 1)/(1+alpha)
   // ai =

   if (subtype == st_Smooth)
      ToNormalizedLattice(a0inv, a1, a2, b0 * gain, b1 * gain, b2 * gain, clipscale(freq, subtype));
   else
      ToCoupledForm(a0inv, a1, a2, b0 * gain, b1 * gain, b2 * gain, clipscale(freq, subtype));
}

void FilterCoefficientMaker::Coeff_LP24(float freq, float reso, int subtype)
{
   float gain = resoscale(reso, subtype);

   boundfreq(freq)

       double Q2inv = Map4PoleResonance((double)reso, (double)freq, subtype);
   float cosi, sinu;
   storage->note_to_omega_ignoring_tuning(freq, sinu, cosi);

   double alpha = sinu * Q2inv;
   if (subtype != st_Smooth)
      alpha = min(alpha, sqrt(1.0 - cosi * cosi) - 0.0001);

   double a0 = 1 + alpha, b0 = (1 - cosi) * 0.5, b1 = 1 - cosi, b2 = (1 - cosi) * 0.5,
          a0inv = 1 / a0, a1 = -2 * cosi, a2 = 1 - alpha;

   if (subtype == st_Smooth)
      ToNormalizedLattice(a0inv, a1, a2, b0 * gain, b1 * gain, b2 * gain, clipscale(freq, subtype));
   else
      ToCoupledForm(a0inv, a1, a2, b0 * gain, b1 * gain, b2 * gain, clipscale(freq, subtype));
}

void FilterCoefficientMaker::Coeff_HP12(float freq, float reso, int subtype)
{
   float gain = resoscale(reso, subtype);

   boundfreq(freq)

       double Q2inv = Map2PoleResonance(reso, freq, subtype);
   float cosi, sinu;
   storage->note_to_omega_ignoring_tuning(freq, sinu, cosi);

   double alpha = sinu * Q2inv;
   if (subtype != 0)
      alpha = min(alpha, sqrt(1.0 - cosi * cosi) - 0.0001);

   double a0 = 1 + alpha, b0 = (1 + cosi) * 0.5, b1 = -(1 + cosi), b2 = (1 + cosi) * 0.5,
          a0inv = 1 / a0, a1 = -2 * cosi, a2 = 1 - alpha;

   if (subtype == st_Smooth)
      ToNormalizedLattice(a0inv, a1, a2, b0 * gain, b1 * gain, b2 * gain, clipscale(freq, subtype));
   else
      ToCoupledForm(a0inv, a1, a2, b0 * gain, b1 * gain, b2 * gain, clipscale(freq, subtype));
}

void FilterCoefficientMaker::Coeff_HP24(float freq, float reso, int subtype)
{
   float gain = resoscale(reso, subtype);

   boundfreq(freq)

       double Q2inv = Map4PoleResonance((double)reso, (double)freq, subtype);
   float cosi, sinu;
   storage->note_to_omega_ignoring_tuning(freq, sinu, cosi);

   double alpha = sinu * Q2inv;
   if (subtype != 0)
      alpha = min(alpha, sqrt(1.0 - cosi * cosi) - 0.0001);

   double a0 = 1 + alpha, b0 = (1 + cosi) * 0.5, b1 = -(1 + cosi), b2 = (1 + cosi) * 0.5,
          a0inv = 1 / a0, a1 = -2 * cosi, a2 = 1 - alpha;

   if (subtype == st_Smooth)
      ToNormalizedLattice(a0inv, a1, a2, b0 * gain, b1 * gain, b2 * gain, clipscale(freq, subtype));
   else
      ToCoupledForm(a0inv, a1, a2, b0 * gain, b1 * gain, b2 * gain, clipscale(freq, subtype));
}

void FilterCoefficientMaker::Coeff_BP12(float freq, float reso, int subtype)
{
   float gain = resoscale(reso, subtype);
   if (subtype == 1)
      gain *= 2.f;

   boundfreq(freq)

       double Q2inv = Map2PoleResonance(reso, freq, subtype);
   double Q = 0.5 / Q2inv;
   float cosi, sinu;
   storage->note_to_omega_ignoring_tuning(freq, sinu, cosi);

   double alpha = sinu * Q2inv;
   if (subtype != 0)
      alpha = min(alpha, sqrt(1.0 - cosi * cosi) - 0.0001);
   double a0 = 1 + alpha, b0, b1, b2, a0inv = 1 / a0, a1 = -2 * cosi, a2 = 1 - alpha;

   {
      b0 = Q * alpha;
      b1 = 0;
      b2 = -Q * alpha;
   }

   if (subtype == st_Smooth)
      ToNormalizedLattice(a0inv, a1, a2, b0 * gain, b1 * gain, b2 * gain, clipscale(freq, subtype));
   else
      ToCoupledForm(a0inv, a1, a2, b0 * gain, b1 * gain, b2 * gain, clipscale(freq, subtype));
}

void FilterCoefficientMaker::Coeff_BR12(float freq, float reso, int subtype)
{
   boundfreq(freq)

       // double Q2inv = (2.5-2.45*limit_range((double)(1-(1-reso)*(1-reso)),0.0,1.0));
       double Q2inv;

   if (subtype == 1)
      Q2inv = (1.00 - 0.99 * limit_range((double)(1 - (1 - reso) * (1 - reso)), 0.0, 1.0));
   else
      Q2inv = (2.5 - 2.49 * limit_range((double)(1 - (1 - reso) * (1 - reso)), 0.0, 1.0));

   float cosi, sinu;
   storage->note_to_omega_ignoring_tuning(freq, sinu, cosi);

   double alpha = sinu * Q2inv, b0 = 1, b1 = -2 * cosi, b2 = 1, a0 = 1 + alpha, a1 = -2 * cosi,
          a2 = 1 - alpha, a0inv = 1 / a0;

   ToNormalizedLattice(a0inv, a1, a2, b0, b1, b2, 0.005);
}

void FilterCoefficientMaker::Coeff_LP4L(float freq, float reso, int subtype)
{
   double gg = limit_range(((double)440 * storage->note_to_pitch_ignoring_tuning(freq) * dsamplerate_os_inv), 0.0,
                           0.187); // gg

   float t_b1 = 1.f - exp(-2 * M_PI * gg);
   float q = min(2.15f * limit_range(reso, 0.f, 1.f), 0.5f / (t_b1 * t_b1 * t_b1 * t_b1));

   float c[n_cm_coeffs];
   memset(c, 0, sizeof(float) * n_cm_coeffs);
   c[0] = (3.f / (3.f - q));
   c[1] = t_b1;
   c[2] = q;

   FromDirect(c);
}

void FilterCoefficientMaker::Coeff_COMB(float freq, float reso, int subtype)
{
   float dtime = (1.f / 440.f) * storage->note_to_pitch_ignoring_tuning(-freq);
   dtime = dtime * dsamplerate_os -
           FIRoffset; // 1 sample for feedback, 1 sample for the IIR-filter without resonance
   dtime = limit_range(dtime, (float)FIRipol_N, (float)MAX_FB_COMB - FIRipol_N);
   reso = ((subtype & 2) ? -1.0f : 1.0f) * limit_range(reso, 0.f, 1.f);

   float c[n_cm_coeffs];
   memset(c, 0, sizeof(float) * n_cm_coeffs);
   c[0] = dtime;
   c[1] = reso;
   c[2] = (subtype & 1) ? 0.0f : 0.5f; // combmix
   c[3] = 1.f - c[2];
   FromDirect(c);
}

void FilterCoefficientMaker::Coeff_SNH(float freq, float reso, int subtype)
{
   float dtime = (1.f / 440.f) * storage->note_to_pitch_ignoring_tuning(-freq) * dsamplerate_os;
   double v1 = 1.0 / dtime;

   float c[n_cm_coeffs];
   memset(c, 0, sizeof(float) * n_cm_coeffs);
   c[0] = v1;
   c[1] = reso;
   FromDirect(c);
}

void FilterCoefficientMaker::FromDirect(float N[n_cm_coeffs])
{
   if (FirstRun)
   {
      memset(dC, 0, sizeof(float) * n_cm_coeffs);
      memcpy(C, N, sizeof(float) * n_cm_coeffs);
      memcpy(tC, N, sizeof(float) * n_cm_coeffs);

      FirstRun = false;
   }
   else
   {
      for (int i = 0; i < n_cm_coeffs; i++)
      {
         tC[i] = (1.f - smooth) * tC[i] + smooth * N[i];
         dC[i] = (tC[i] - C[i]) * BLOCK_SIZE_OS_INV;
      }
   }
}

void FilterCoefficientMaker::ToNormalizedLattice(
    double a0inv, double a1, double a2, double b0, double b1, double b2, double g)
{
   b0 *= a0inv;
   b1 *= a0inv;
   b2 *= a0inv;
   a1 *= a0inv;
   a2 *= a0inv;

   float N[n_cm_coeffs];
   memset(N, 0, sizeof(float) * n_cm_coeffs);

   double k1 = a1 / (1.0 + a2);
   double k2 = a2;

   double q1 = 1.0 - k1 * k1;
   double q2 = 1.0 - k2 * k2;
   q1 = sqrt(fabs(q1));
   q2 = sqrt(fabs(q2));

   double v3 = b2;
   double v2 = (b1 - a1 * v3) / q2;
   double v1 = (b0 - k1 * v2 * q2 - k2 * v3) / (q1 * q2);

   N[0] = k1;
   N[1] = k2;
   N[2] = q1;
   N[3] = q2;
   N[4] = v1;
   N[5] = v2;
   N[6] = v3;
   N[7] = g;

   FromDirect(N);
}
void FilterCoefficientMaker::ToCoupledForm(
    double a0inv, double a1, double a2, double b0, double b1, double b2, double g)
{
   b0 *= a0inv;
   b1 *= a0inv;
   b2 *= a0inv;
   a1 *= a0inv;
   a2 *= a0inv;

   float N[n_cm_coeffs];
   memset(N, 0, sizeof(float) * n_cm_coeffs);

   double ar, ai;
   double sq = a1 * a1 - 4.0 * a2;

   ar = 0.5 * -a1;
   sq = min(0.0, sq);
   ai = 0.5 * sqrt(-sq);
   ai = max(ai, 8.0 * 1.192092896e-07F);

   double bb1 = b1 - a1 * b0;
   double bb2 = b2 - a2 * b0;

   double d = b0;
   double c1 = bb1;
   double c2 = (bb1 * ar + bb2) / ai;

   double scaler = 1.f; // 0.01 + 0.99*sqrt(c1*c1);

   N[0] = ar;
   N[1] = ai;
   N[2] = 1.f; // scaler;
   N[4] = c1;  // /scaler;
   N[5] = c2;  // /scaler;
   N[6] = d;
   N[7] = g;

   FromDirect(N);
}

void FilterCoefficientMaker::Reset()
{
   FirstRun = true;
   memset(C, 0, sizeof(float) * n_cm_coeffs);
   memset(dC, 0, sizeof(float) * n_cm_coeffs);
   memset(tC, 0, sizeof(float) * n_cm_coeffs);

   storage = nullptr;
}
