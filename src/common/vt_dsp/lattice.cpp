#include <emmintrin.h>
#include "lattice.h"
#include "basic_dsp.h"

// iir_lattice_sd =

void iir_lattice_pd_SSE2(lattice_pd& d, double* __restrict Data, int NSamples)
{
   const __m128d v0_1 = _mm_set_sd(0.1);
   const __m128d v1_0 = _mm_set_sd(1.0);

   __m128d r1 = d.reg[1];
   __m128d r0 = d.reg[0];
   __m128d regmult = d.reg[2];

   __m128d K1 = d.v.K1;
   __m128d K2 = d.v.K2;
   __m128d V1 = d.v.V1;
   __m128d V2 = d.v.V2;
   __m128d V3 = d.v.V3;
   __m128d CG = d.v.ClipGain;

   for (int i = 0; i < NSamples; i++)
   {
      __m128d in = _mm_load_pd(&Data[i << 1]);
      K2 = _mm_add_pd(K2, d.dv.K2);
      K1 = _mm_add_pd(K1, d.dv.K1);
      __m128d f1 = _mm_sub_pd(in, _mm_mul_pd(r1, K2));
      __m128d f0 = _mm_mul_pd(r0, K1);
      f0 = _mm_sub_pd(f1, f0);
      __m128d g1 = _mm_mul_pd(f1, K2);
      g1 = _mm_add_pd(r1, g1);
      __m128d g0 = _mm_mul_pd(f0, K1);
      g0 = _mm_add_pd(r0, g0);

      r1 = _mm_mul_pd(g0, regmult);

      V2 = _mm_add_pd(V2, d.dv.V2);
      r0 = _mm_mul_pd(f0, regmult);
      V3 = _mm_add_pd(V3, d.dv.V3);
      V1 = _mm_add_pd(V1, d.dv.V1);

      __m128d y = _mm_mul_pd(f0, V1);
      y = _mm_add_pd(y, _mm_mul_pd(g1, V3));
      y = _mm_add_pd(y, _mm_mul_pd(g0, V2));

      CG = _mm_add_pd(CG, d.dv.ClipGain);
      _mm_store_pd(Data, y);

      y = _mm_mul_pd(y, y);
      y = _mm_mul_pd(y, CG);
      y = _mm_sub_pd(v1_0, y);
      regmult = _mm_max_pd(v0_1, y);
   }

   d.v.ClipGain = CG;
   d.v.K2 = K2;
   d.v.K1 = K1;
   d.v.V3 = V3;
   d.v.V2 = V2;
   d.v.V1 = V1;

   d.reg[1] = r1;
   d.reg[0] = r0;

   d.reg[2] = regmult;
}

void iir_lattice_sd_SSE2(lattice_sd& d, double* __restrict Data, int NSamples)
{
   const double v0_1 = 0.1;
   const double v1_0 = 1.0;

   __m128d r1 = _mm_load_sd(&d.reg[1]);
   __m128d r0 = _mm_load_sd(&d.reg[0]);
   __m128d regmult = _mm_load_sd(&d.reg[2]);

   __m128d K1 = _mm_load_sd(&d.v.K1);
   __m128d K2 = _mm_load_sd(&d.v.K2);
   __m128d V1 = _mm_load_sd(&d.v.V1);
   __m128d V2 = _mm_load_sd(&d.v.V2);
   __m128d V3 = _mm_load_sd(&d.v.V3);
   __m128d CG = _mm_load_sd(&d.v.ClipGain);

   for (int i = 0; i < NSamples; i++)
   {
      __m128d in = _mm_load_sd(&Data[i]);
      K2 = _mm_add_sd(K2, _mm_load_sd(&d.dv.K2));
      K1 = _mm_add_sd(K1, _mm_load_sd(&d.dv.K1));
      __m128d f1 = _mm_sub_sd(in, _mm_mul_sd(r1, K2));
      __m128d f0 = _mm_mul_sd(r0, K1);
      f0 = _mm_sub_sd(f1, f0);
      __m128d g1 = _mm_mul_sd(f1, K2);
      g1 = _mm_add_sd(r1, g1);
      __m128d g0 = _mm_mul_sd(f0, K1);
      g0 = _mm_add_sd(r0, g0);

      r1 = _mm_mul_sd(g0, regmult);
      r0 = _mm_mul_sd(f0, regmult);

      V2 = _mm_add_sd(V2, _mm_load_sd(&d.dv.V2));
      V3 = _mm_add_sd(V3, _mm_load_sd(&d.dv.V3));
      V1 = _mm_add_sd(V1, _mm_load_sd(&d.dv.V1));

      __m128d y = _mm_mul_sd(f0, _mm_load_sd(&d.v.V1));
      y = _mm_add_sd(y, _mm_mul_sd(g1, _mm_load_sd(&d.v.V3)));
      y = _mm_add_sd(y, _mm_mul_sd(g0, _mm_load_sd(&d.v.V2)));

      CG = _mm_add_sd(CG, _mm_load_sd(&d.dv.ClipGain));
      _mm_store_sd(&Data[i], y);

      regmult = _mm_max_sd(_mm_load_sd(&v0_1),
                           _mm_sub_sd(_mm_load_sd(&v1_0), _mm_mul_sd(_mm_mul_sd(y, y), CG)));
   }

   _mm_store_sd(&d.v.ClipGain, CG);
   _mm_store_sd(&d.v.K2, K2);
   _mm_store_sd(&d.v.K1, K1);
   _mm_store_sd(&d.v.V3, V3);
   _mm_store_sd(&d.v.V2, V2);
   _mm_store_sd(&d.v.V1, V1);

   _mm_store_sd(&d.reg[1], r1);
   _mm_store_sd(&d.reg[0], r0);

   _mm_store_sd(&d.reg[2], regmult);
}

void iir_lattice_sd_x87(lattice_sd& d, double* __restrict Data, int NSamples)
{
   for (int i = 0; i < NSamples; i++)
   {
      d.v.K1 += d.dv.K1;
      d.v.K2 += d.dv.K2;

      double f1 = Data[i] - d.v.K2 * d.reg[1];
      double f0 = f1 - d.v.K1 * d.reg[0];
      double g1 = d.v.K2 * f1 + d.reg[1];
      double g0 = d.v.K1 * f0 + d.reg[0];

      d.v.V1 += d.dv.V1;
      d.v.V2 += d.dv.V2;
      d.v.V3 += d.dv.V3;

      double y1 = (d.v.V3 * g1 + d.v.V1 * f0) + d.v.V2 * g0;
      Data[i] = y1;

      d.reg[0] = f0 * d.reg[2];
      d.reg[1] = g0 * d.reg[2];
      d.v.ClipGain += d.dv.ClipGain;
      double t = 1.0 - d.v.ClipGain * y1 * y1;
      d.reg[2] = max(0.1, t);
   }
}

// TODO måste testas
void iir_lattice_pd_serial_SSE2(lattice_pd& d, double* __restrict Data, int NSamples)
{
   const __m128d v0_1 = _mm_set_sd(0.1);
   const __m128d v1_0 = _mm_set_sd(1.0);

   __m128d r1 = d.reg[1];
   __m128d r0 = d.reg[0];
   __m128d regmult = d.reg[2];

   __m128d K1 = d.v.K1;
   __m128d K2 = d.v.K2;
   __m128d V1 = d.v.V1;
   __m128d V2 = d.v.V2;
   __m128d V3 = d.v.V3;
   __m128d CG = d.v.ClipGain;
   __m128d in = d.reg[4];

   for (int i = 0; i < NSamples; i++)
   {
      in = _mm_loadl_pd(in, &Data[i]);
      K2 = _mm_add_pd(K2, d.dv.K2);
      K1 = _mm_add_pd(K1, d.dv.K1);
      __m128d f1 = _mm_sub_pd(in, _mm_mul_pd(r1, K2));
      __m128d f0 = _mm_mul_pd(r0, K1);
      f0 = _mm_sub_pd(f1, f0);
      __m128d g1 = _mm_mul_pd(f1, K2);
      g1 = _mm_add_pd(r1, g1);
      __m128d g0 = _mm_mul_pd(f0, K1);
      g0 = _mm_add_pd(r0, g0);

      r1 = _mm_mul_pd(g0, regmult);

      V2 = _mm_add_pd(V2, d.dv.V2);
      r0 = _mm_mul_pd(f0, regmult);
      V3 = _mm_add_pd(V3, d.dv.V3);
      V1 = _mm_add_pd(V1, d.dv.V1);

      __m128d y = _mm_mul_pd(f0, V1);
      y = _mm_add_pd(y, _mm_mul_pd(g1, V3));
      y = _mm_add_pd(y, _mm_mul_pd(g0, V2));

      CG = _mm_add_pd(CG, d.dv.ClipGain);
      _mm_store_pd(Data, y);
      in = _mm_unpacklo_pd(in, y);

      y = _mm_mul_pd(y, y);
      y = _mm_mul_pd(y, CG);
      y = _mm_sub_pd(v1_0, y);
      regmult = _mm_max_pd(v0_1, y);
   }

   d.v.ClipGain = CG;
   d.v.K2 = K2;
   d.v.K1 = K1;
   d.v.V3 = V3;
   d.v.V2 = V2;
   d.v.V1 = V1;

   d.reg[1] = r1;
   d.reg[0] = r0;

   d.reg[2] = regmult;
   d.reg[4] = in;
}

double map_resonance_to_2Qinv(double reso, double freq)
{
   reso *= max(0.0, 1.0 - max(0.0, (freq - 58) * 0.05));
   double Qinv = (1.0 - 1.05 * limit_range((double)(1 - (1 - reso) * (1 - reso)), 0.0, 1.0));
   return Qinv;
}

double resoscale(double reso)
{
   return (1 - 0.5 * reso * reso);
   //(1-0.5*reso*(0.5+reso)); // old V (surge v1.1)
}

void coeff_LP2_sd(lattice_sd& d, double freq, double reso)
{
   float gain = resoscale(reso);

   double Q2inv = map_resonance_to_2Qinv(reso, freq);

   double w0 = 2.0 * 3.14 * 440.0 * powf(2.0, freq) / 44100.0;
   double sinu = sin(w0);
   double cosi = cos(w0);

   double alpha = sinu * Q2inv, a0 = 1.0 + alpha, b0 = (1.0 - cosi) * 0.5, b1 = 1.0 - cosi,
          b2 = (1.0 - cosi) * 0.5, a0inv = 1.0 / a0, a1 = -2 * cosi, a2 = 1.0 - alpha;

   b0 *= a0inv;
   b1 *= a0inv;
   b2 *= a0inv;
   a1 *= a0inv;
   a2 *= a0inv;

   // update the feedback coefficients
   double k1 = a1 / (1.0 + a2);
   double k2 = a2;
   // update the output coefficients
   double v3 = b2;
   double v2 = b1 - (k1 * k2 + k1) * v3;
   double v1 = b0 - k1 * v2 - k2 * v3;

   d.v.K2 = k2;
   d.v.K1 = k1;
   d.v.V3 = v3;
   d.v.V2 = v2;
   d.v.V1 = v1;
   d.v.ClipGain = gain;

   d.dv.K2 = 0;
   d.dv.K1 = 0;
   d.dv.V3 = 0;
   d.dv.V2 = 0;
   d.dv.V1 = 0;
   d.dv.ClipGain = 0;
}