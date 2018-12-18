#include "CpuArchitecture.h"
#include "QuadFilterUnit.h"
#include "SurgeStorage.h"
#include <vt_dsp/basic_dsp.h>

#if PPC
vFloat m0 = (vFloat)0.f;

union vSInt32Union
{
   vSInt32 V;
   int Int[4];
};

#endif

#if PPC
vFloat SVFLP12Aquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   f->C[0] = vec_add(f->C[0], f->dC[0]); // F1
   f->C[1] = vec_add(f->C[1], f->dC[1]); // Q1

   vFloat L = vec_madd(f->C[0], f->R[0], f->R[1]);
   vFloat H = vec_sub(in, vec_madd(f->C[1], f->R[0], L));
   vFloat B = vec_madd(f->C[0], H, f->R[0]);

   vFloat L2 = vec_madd(f->C[0], B, L);
   vFloat H2 = vec_sub(in, vec_madd(f->C[1], B, L2));
   vFloat B2 = vec_madd(f->C[0], H2, B);

   f->R[0] = vec_madd(B2, f->R[2], m0);
   f->R[1] = vec_madd(L2, f->R[2], m0);

   f->C[2] = vec_add(f->C[2], f->dC[2]);
   const vFloat m01 = (vFloat)(0.1f);
   const vFloat m1 = (vFloat)(1.0f);
   f->R[2] = vec_max(m01, vec_nmsub(f->C[2], vec_madd(B, B, m0), m1));

   f->C[3] = vec_add(f->C[3], f->dC[3]); // Gain
   return vec_madd(L2, f->C[3], m0);
}
#else
__m128 SVFLP12Aquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   f->C[0] = _mm_add_ps(f->C[0], f->dC[0]); // F1
   f->C[1] = _mm_add_ps(f->C[1], f->dC[1]); // Q1

   __m128 L = _mm_add_ps(f->R[1], _mm_mul_ps(f->C[0], f->R[0]));
   __m128 H = _mm_sub_ps(_mm_sub_ps(in, L), _mm_mul_ps(f->C[1], f->R[0]));
   __m128 B = _mm_add_ps(f->R[0], _mm_mul_ps(f->C[0], H));

   __m128 L2 = _mm_add_ps(L, _mm_mul_ps(f->C[0], B));
   __m128 H2 = _mm_sub_ps(_mm_sub_ps(in, L2), _mm_mul_ps(f->C[1], B));
   __m128 B2 = _mm_add_ps(B, _mm_mul_ps(f->C[0], H2));

   f->R[0] = _mm_mul_ps(B2, f->R[2]);
   f->R[1] = _mm_mul_ps(L2, f->R[2]);

   f->C[2] = _mm_add_ps(f->C[2], f->dC[2]);
   const __m128 m01 = _mm_set1_ps(0.1f);
   const __m128 m1 = _mm_set1_ps(1.0f);
   f->R[2] = _mm_max_ps(m01, _mm_sub_ps(m1, _mm_mul_ps(f->C[2], _mm_mul_ps(B, B))));

   f->C[3] = _mm_add_ps(f->C[3], f->dC[3]); // Gain
   return _mm_mul_ps(L2, f->C[3]);
}
#endif
#if PPC
vFloat SVFLP24Aquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   f->C[0] = vec_add(f->C[0], f->dC[0]); // F1
   f->C[1] = vec_add(f->C[1], f->dC[1]); // Q1

   vFloat L = vec_madd(f->C[0], f->R[0], f->R[1]);
   vFloat H = vec_sub(in, vec_madd(f->C[1], f->R[0], L));
   vFloat B = vec_madd(f->C[0], H, f->R[0]);
   L = vec_madd(f->C[0], B, L);
   H = vec_sub(in, vec_madd(f->C[1], B, L));
   B = vec_madd(f->C[0], H, B);

   f->R[0] = vec_madd(B, f->R[2], m0);
   f->R[1] = vec_madd(L, f->R[2], m0);

   in = L;

   L = vec_madd(f->C[0], f->R[3], f->R[4]);
   H = vec_sub(in, vec_madd(f->C[1], f->R[3], L));
   B = vec_madd(f->C[0], H, f->R[3]);
   L = vec_madd(f->C[0], B, L);
   H = vec_sub(in, vec_madd(f->C[1], B, L));
   B = vec_madd(f->C[0], H, B);

   f->R[3] = vec_madd(B, f->R[2], m0);
   f->R[4] = vec_madd(L, f->R[2], m0);

   f->C[2] = vec_add(f->C[2], f->dC[2]);
   const vFloat m01 = (vFloat)(0.1f);
   const vFloat m1 = (vFloat)(1.0f);
   f->R[2] = vec_max(m01, vec_nmsub(f->C[2], vec_madd(B, B, m0), m1));

   f->C[3] = vec_add(f->C[3], f->dC[3]); // Gain
   return vec_madd(L, f->C[3], m0);
}
#else
__m128 SVFLP24Aquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   f->C[0] = _mm_add_ps(f->C[0], f->dC[0]); // F1
   f->C[1] = _mm_add_ps(f->C[1], f->dC[1]); // Q1

   __m128 L = _mm_add_ps(f->R[1], _mm_mul_ps(f->C[0], f->R[0]));
   __m128 H = _mm_sub_ps(_mm_sub_ps(in, L), _mm_mul_ps(f->C[1], f->R[0]));
   __m128 B = _mm_add_ps(f->R[0], _mm_mul_ps(f->C[0], H));

   L = _mm_add_ps(L, _mm_mul_ps(f->C[0], B));
   H = _mm_sub_ps(_mm_sub_ps(in, L), _mm_mul_ps(f->C[1], B));
   B = _mm_add_ps(B, _mm_mul_ps(f->C[0], H));

   f->R[0] = _mm_mul_ps(B, f->R[2]);
   f->R[1] = _mm_mul_ps(L, f->R[2]);

   in = L;

   L = _mm_add_ps(f->R[4], _mm_mul_ps(f->C[0], f->R[3]));
   H = _mm_sub_ps(_mm_sub_ps(in, L), _mm_mul_ps(f->C[1], f->R[3]));
   B = _mm_add_ps(f->R[3], _mm_mul_ps(f->C[0], H));

   L = _mm_add_ps(L, _mm_mul_ps(f->C[0], B));
   H = _mm_sub_ps(_mm_sub_ps(in, L), _mm_mul_ps(f->C[1], B));
   B = _mm_add_ps(B, _mm_mul_ps(f->C[0], H));

   f->R[3] = _mm_mul_ps(B, f->R[2]);
   f->R[4] = _mm_mul_ps(L, f->R[2]);

   f->C[2] = _mm_add_ps(f->C[2], f->dC[2]);
   const __m128 m01 = _mm_set1_ps(0.1f);
   const __m128 m1 = _mm_set1_ps(1.0f);
   f->R[2] = _mm_max_ps(m01, _mm_sub_ps(m1, _mm_mul_ps(f->C[2], _mm_mul_ps(B, B))));

   f->C[3] = _mm_add_ps(f->C[3], f->dC[3]); // Gain
   return _mm_mul_ps(L, f->C[3]);
}
#endif
#if PPC
vFloat SVFHP24Aquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   f->C[0] = vec_add(f->C[0], f->dC[0]); // F1
   f->C[1] = vec_add(f->C[1], f->dC[1]); // Q1

   vFloat L = vec_madd(f->C[0], f->R[0], f->R[1]);
   vFloat H = vec_sub(in, vec_madd(f->C[1], f->R[0], L));
   vFloat B = vec_madd(f->C[0], H, f->R[0]);
   L = vec_madd(f->C[0], B, L);
   H = vec_sub(in, vec_madd(f->C[1], f->R[3], L));
   B = vec_madd(f->C[0], H, B);

   f->R[0] = vec_madd(B, f->R[2], m0);
   f->R[1] = vec_madd(L, f->R[2], m0);

   in = H;

   L = vec_madd(f->C[0], f->R[3], f->R[4]);
   H = vec_sub(in, vec_madd(f->C[1], f->R[3], L));
   B = vec_madd(f->C[0], H, f->R[3]);
   L = vec_madd(f->C[0], B, L);
   H = vec_sub(in, vec_madd(f->C[1], B, L));
   B = vec_madd(f->C[0], H, B);

   f->R[3] = vec_madd(B, f->R[2], m0);
   f->R[4] = vec_madd(L, f->R[2], m0);

   f->C[2] = vec_add(f->C[2], f->dC[2]);
   const vFloat m01 = (vFloat)(0.1f);
   const vFloat m1 = (vFloat)(1.0f);
   f->R[2] = vec_max(m01, vec_nmsub(f->C[2], vec_madd(B, B, m0), m1));

   f->C[3] = vec_add(f->C[3], f->dC[3]); // Gain
   return vec_madd(H, f->C[3], m0);
}
#else
__m128 SVFHP24Aquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   f->C[0] = _mm_add_ps(f->C[0], f->dC[0]); // F1
   f->C[1] = _mm_add_ps(f->C[1], f->dC[1]); // Q1

   __m128 L = _mm_add_ps(f->R[1], _mm_mul_ps(f->C[0], f->R[0]));
   __m128 H = _mm_sub_ps(_mm_sub_ps(in, L), _mm_mul_ps(f->C[1], f->R[0]));
   __m128 B = _mm_add_ps(f->R[0], _mm_mul_ps(f->C[0], H));

   L = _mm_add_ps(L, _mm_mul_ps(f->C[0], B));
   H = _mm_sub_ps(_mm_sub_ps(in, L), _mm_mul_ps(f->C[1], B));
   B = _mm_add_ps(B, _mm_mul_ps(f->C[0], H));

   f->R[0] = _mm_mul_ps(B, f->R[2]);
   f->R[1] = _mm_mul_ps(L, f->R[2]);

   in = H;

   L = _mm_add_ps(f->R[4], _mm_mul_ps(f->C[0], f->R[3]));
   H = _mm_sub_ps(_mm_sub_ps(in, L), _mm_mul_ps(f->C[1], f->R[3]));
   B = _mm_add_ps(f->R[3], _mm_mul_ps(f->C[0], H));

   L = _mm_add_ps(L, _mm_mul_ps(f->C[0], B));
   H = _mm_sub_ps(_mm_sub_ps(in, L), _mm_mul_ps(f->C[1], B));
   B = _mm_add_ps(B, _mm_mul_ps(f->C[0], H));

   f->R[3] = _mm_mul_ps(B, f->R[2]);
   f->R[4] = _mm_mul_ps(L, f->R[2]);

   f->C[2] = _mm_add_ps(f->C[2], f->dC[2]);
   const __m128 m01 = _mm_set1_ps(0.1f);
   const __m128 m1 = _mm_set1_ps(1.0f);
   f->R[2] = _mm_max_ps(m01, _mm_sub_ps(m1, _mm_mul_ps(f->C[2], _mm_mul_ps(B, B))));

   f->C[3] = _mm_add_ps(f->C[3], f->dC[3]); // Gain
   return _mm_mul_ps(H, f->C[3]);
}
#endif

#if PPC
vFloat SVFBP24Aquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   f->C[0] = vec_add(f->C[0], f->dC[0]); // F1
   f->C[1] = vec_add(f->C[1], f->dC[1]); // Q1

   vFloat L = vec_madd(f->C[0], f->R[0], f->R[1]);
   vFloat H = vec_sub(in, vec_madd(f->C[1], f->R[0], L));
   vFloat B = vec_madd(f->C[0], H, f->R[0]);
   L = vec_madd(f->C[0], B, L);
   H = vec_sub(in, vec_madd(f->C[1], f->R[3], L));
   B = vec_madd(f->C[0], H, B);

   f->R[0] = vec_madd(B, f->R[2], m0);
   f->R[1] = vec_madd(L, f->R[2], m0);

   in = B;

   L = vec_madd(f->C[0], f->R[3], f->R[4]);
   H = vec_sub(in, vec_madd(f->C[1], f->R[3], L));
   B = vec_madd(f->C[0], H, f->R[3]);
   L = vec_madd(f->C[0], B, L);
   H = vec_sub(in, vec_madd(f->C[1], B, L));
   B = vec_madd(f->C[0], H, B);

   f->R[3] = vec_madd(B, f->R[2], m0);
   f->R[4] = vec_madd(L, f->R[2], m0);

   f->C[2] = vec_add(f->C[2], f->dC[2]);
   const vFloat m01 = (vFloat)(0.1f);
   const vFloat m1 = (vFloat)(1.0f);
   f->R[2] = vec_max(m01, vec_nmsub(f->C[2], vec_madd(B, B, m0), m1));

   f->C[3] = vec_add(f->C[3], f->dC[3]); // Gain
   return vec_madd(B, f->C[3], m0);
}
#else
__m128 SVFBP24Aquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   f->C[0] = _mm_add_ps(f->C[0], f->dC[0]); // F1
   f->C[1] = _mm_add_ps(f->C[1], f->dC[1]); // Q1

   __m128 L = _mm_add_ps(f->R[1], _mm_mul_ps(f->C[0], f->R[0]));
   __m128 H = _mm_sub_ps(_mm_sub_ps(in, L), _mm_mul_ps(f->C[1], f->R[0]));
   __m128 B = _mm_add_ps(f->R[0], _mm_mul_ps(f->C[0], H));

   L = _mm_add_ps(L, _mm_mul_ps(f->C[0], B));
   H = _mm_sub_ps(_mm_sub_ps(in, L), _mm_mul_ps(f->C[1], B));
   B = _mm_add_ps(B, _mm_mul_ps(f->C[0], H));

   f->R[0] = _mm_mul_ps(B, f->R[2]);
   f->R[1] = _mm_mul_ps(L, f->R[2]);

   in = B;

   L = _mm_add_ps(f->R[4], _mm_mul_ps(f->C[0], f->R[3]));
   H = _mm_sub_ps(_mm_sub_ps(in, L), _mm_mul_ps(f->C[1], f->R[3]));
   B = _mm_add_ps(f->R[3], _mm_mul_ps(f->C[0], H));

   L = _mm_add_ps(L, _mm_mul_ps(f->C[0], B));
   H = _mm_sub_ps(_mm_sub_ps(in, L), _mm_mul_ps(f->C[1], B));
   B = _mm_add_ps(B, _mm_mul_ps(f->C[0], H));

   f->R[3] = _mm_mul_ps(B, f->R[2]);
   f->R[4] = _mm_mul_ps(L, f->R[2]);

   f->C[2] = _mm_add_ps(f->C[2], f->dC[2]);
   const __m128 m01 = _mm_set1_ps(0.1f);
   const __m128 m1 = _mm_set1_ps(1.0f);
   f->R[2] = _mm_max_ps(m01, _mm_sub_ps(m1, _mm_mul_ps(f->C[2], _mm_mul_ps(B, B))));

   f->C[3] = _mm_add_ps(f->C[3], f->dC[3]); // Gain
   return _mm_mul_ps(B, f->C[3]);
}
#endif

#if PPC
vFloat SVFHP12Aquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   f->C[0] = vec_add(f->C[0], f->dC[0]); // F1
   f->C[1] = vec_add(f->C[1], f->dC[1]); // Q1

   vFloat L = vec_madd(f->C[0], f->R[0], f->R[1]);
   vFloat H = vec_sub(in, vec_madd(f->C[1], f->R[0], L));
   vFloat B = vec_madd(f->C[0], H, f->R[0]);

   vFloat L2 = vec_madd(f->C[0], B, L);
   vFloat H2 = vec_sub(in, vec_madd(f->C[1], B, L2));
   vFloat B2 = vec_madd(f->C[0], H2, B);

   f->R[0] = vec_madd(B2, f->R[2], m0);
   f->R[1] = vec_madd(L2, f->R[2], m0);

   f->C[2] = vec_add(f->C[2], f->dC[2]);
   const vFloat m01 = (vFloat)(0.1f);
   const vFloat m1 = (vFloat)(1.0f);
   f->R[2] = vec_max(m01, vec_nmsub(f->C[2], vec_madd(B, B, m0), m1));

   f->C[3] = vec_add(f->C[3], f->dC[3]); // Gain
   return vec_madd(H2, f->C[3], m0);
}
#else
__m128 SVFHP12Aquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   f->C[0] = _mm_add_ps(f->C[0], f->dC[0]); // F1
   f->C[1] = _mm_add_ps(f->C[1], f->dC[1]); // Q1

   __m128 L = _mm_add_ps(f->R[1], _mm_mul_ps(f->C[0], f->R[0]));
   __m128 H = _mm_sub_ps(_mm_sub_ps(in, L), _mm_mul_ps(f->C[1], f->R[0]));
   __m128 B = _mm_add_ps(f->R[0], _mm_mul_ps(f->C[0], H));

   __m128 L2 = _mm_add_ps(L, _mm_mul_ps(f->C[0], B));
   __m128 H2 = _mm_sub_ps(_mm_sub_ps(in, L2), _mm_mul_ps(f->C[1], B));
   __m128 B2 = _mm_add_ps(B, _mm_mul_ps(f->C[0], H2));

   f->R[0] = _mm_mul_ps(B2, f->R[2]);
   f->R[1] = _mm_mul_ps(L2, f->R[2]);

   f->C[2] = _mm_add_ps(f->C[2], f->dC[2]);
   const __m128 m01 = _mm_set1_ps(0.1f);
   const __m128 m1 = _mm_set1_ps(1.0f);
   f->R[2] = _mm_max_ps(m01, _mm_sub_ps(m1, _mm_mul_ps(f->C[2], _mm_mul_ps(B, B))));

   f->C[3] = _mm_add_ps(f->C[3], f->dC[3]); // Gain
   return _mm_mul_ps(H2, f->C[3]);
}
#endif
#if PPC
vFloat SVFBP12Aquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   f->C[0] = vec_add(f->C[0], f->dC[0]); // F1
   f->C[1] = vec_add(f->C[1], f->dC[1]); // Q1

   vFloat L = vec_madd(f->C[0], f->R[0], f->R[1]);
   vFloat H = vec_sub(in, vec_madd(f->C[1], f->R[0], L));
   vFloat B = vec_madd(f->C[0], H, f->R[0]);

   vFloat L2 = vec_madd(f->C[0], B, L);
   vFloat H2 = vec_sub(in, vec_madd(f->C[1], B, L2));
   vFloat B2 = vec_madd(f->C[0], H2, B);

   f->R[0] = vec_madd(B2, f->R[2], m0);
   f->R[1] = vec_madd(L2, f->R[2], m0);

   f->C[2] = vec_add(f->C[2], f->dC[2]);
   const vFloat m01 = (vFloat)(0.1f);
   const vFloat m1 = (vFloat)(1.0f);
   f->R[2] = vec_max(m01, vec_nmsub(f->C[2], vec_madd(B, B, m0), m1));

   f->C[3] = vec_add(f->C[3], f->dC[3]); // Gain
   return vec_madd(B2, f->C[3], m0);
}
#else
__m128 SVFBP12Aquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   f->C[0] = _mm_add_ps(f->C[0], f->dC[0]); // F1
   f->C[1] = _mm_add_ps(f->C[1], f->dC[1]); // Q1

   __m128 L = _mm_add_ps(f->R[1], _mm_mul_ps(f->C[0], f->R[0]));
   __m128 H = _mm_sub_ps(_mm_sub_ps(in, L), _mm_mul_ps(f->C[1], f->R[0]));
   __m128 B = _mm_add_ps(f->R[0], _mm_mul_ps(f->C[0], H));

   __m128 L2 = _mm_add_ps(L, _mm_mul_ps(f->C[0], B));
   __m128 H2 = _mm_sub_ps(_mm_sub_ps(in, L2), _mm_mul_ps(f->C[1], B));
   __m128 B2 = _mm_add_ps(B, _mm_mul_ps(f->C[0], H2));

   f->R[0] = _mm_mul_ps(B2, f->R[2]);
   f->R[1] = _mm_mul_ps(L2, f->R[2]);

   f->C[2] = _mm_add_ps(f->C[2], f->dC[2]);
   const __m128 m01 = _mm_set1_ps(0.1f);
   const __m128 m1 = _mm_set1_ps(1.0f);
   f->R[2] = _mm_max_ps(m01, _mm_sub_ps(m1, _mm_mul_ps(f->C[2], _mm_mul_ps(B, B))));

   f->C[3] = _mm_add_ps(f->C[3], f->dC[3]); // Gain
   return _mm_mul_ps(B2, f->C[3]);
}
#endif
#if PPC
#else
__m128 IIR12Aquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   f->C[1] = _mm_add_ps(f->C[1], f->dC[1]);                                       // K2
   f->C[3] = _mm_add_ps(f->C[3], f->dC[3]);                                       // Q2
   __m128 f2 = _mm_sub_ps(_mm_mul_ps(f->C[3], in), _mm_mul_ps(f->C[1], f->R[1])); // Q2*in - K2*R1
   __m128 g2 = _mm_add_ps(_mm_mul_ps(f->C[1], in), _mm_mul_ps(f->C[3], f->R[1])); // K2*in + Q2*R1

   f->C[0] = _mm_add_ps(f->C[0], f->dC[0]);                                       // K1
   f->C[2] = _mm_add_ps(f->C[2], f->dC[2]);                                       // Q1
   __m128 f1 = _mm_sub_ps(_mm_mul_ps(f->C[2], f2), _mm_mul_ps(f->C[0], f->R[0])); // Q1*f2 - K1*R0
   __m128 g1 = _mm_add_ps(_mm_mul_ps(f->C[0], f2), _mm_mul_ps(f->C[2], f->R[0])); // K1*f2 + Q1*R0

   f->C[4] = _mm_add_ps(f->C[4], f->dC[4]); // V1
   f->C[5] = _mm_add_ps(f->C[5], f->dC[5]); // V2
   f->C[6] = _mm_add_ps(f->C[6], f->dC[6]); // V3
   __m128 y = _mm_add_ps(_mm_add_ps(_mm_mul_ps(f->C[6], g2), _mm_mul_ps(f->C[5], g1)),
                         _mm_mul_ps(f->C[4], f1));

   f->R[0] = f1;
   f->R[1] = g1;

   return y;
}
#endif
#if PPC
vFloat IIR12Bquad(QuadFilterUnitState* __restrict f, vFloat in)
{
   vFloat f2 = vec_nmsub(f->C[1], f->R[1], vec_madd(f->C[3], in, m0)); // Q2*in - K2*R1
   f->C[1] = vec_add(f->C[1], f->dC[1]);                               // K2
   f->C[3] = vec_add(f->C[3], f->dC[3]);                               // Q2
   vFloat g2 = vec_madd(f->C[1], in, vec_madd(f->C[3], f->R[1], m0));  // K2*in + Q2*R1

   vFloat f1 = vec_nmsub(f->C[0], f->R[0], vec_madd(f->C[2], f2, m0)); // Q1*f2 - K1*R0
   f->C[0] = vec_add(f->C[0], f->dC[0]);                               // K1
   f->C[2] = vec_add(f->C[2], f->dC[2]);                               // Q1
   vFloat g1 = vec_madd(f->C[0], f2, vec_madd(f->C[2], f->R[0], m0));  // K1*f2 + Q1*R0

   f->C[4] = vec_add(f->C[4], f->dC[4]); // V1
   f->C[5] = vec_add(f->C[5], f->dC[5]); // V2
   f->C[6] = vec_add(f->C[6], f->dC[6]); // V3
   vFloat y = vec_madd(f->C[6], g2, m0);
   y = vec_madd(f->C[5], g1, y);
   y = vec_madd(f->C[4], f1, y);

   f->R[0] = vec_madd(f1, f->R[2], m0);
   f->R[1] = vec_madd(g1, f->R[2], m0);

   f->C[7] = vec_add(f->C[7], f->dC[7]); // Clipgain
   const vFloat m01 = (vFloat)(0.1f);
   const vFloat m1 = (vFloat)(1.0f);

   f->R[2] = vec_max(m01, vec_nmsub(f->C[7], vec_madd(y, y, m0), m1));

   return y;
}
#else
__m128 IIR12Bquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   __m128 f2 = _mm_sub_ps(_mm_mul_ps(f->C[3], in), _mm_mul_ps(f->C[1], f->R[1])); // Q2*in - K2*R1
   f->C[1] = _mm_add_ps(f->C[1], f->dC[1]);                                       // K2
   f->C[3] = _mm_add_ps(f->C[3], f->dC[3]);                                       // Q2
   __m128 g2 = _mm_add_ps(_mm_mul_ps(f->C[1], in), _mm_mul_ps(f->C[3], f->R[1])); // K2*in + Q2*R1

   __m128 f1 = _mm_sub_ps(_mm_mul_ps(f->C[2], f2), _mm_mul_ps(f->C[0], f->R[0])); // Q1*f2 - K1*R0
   f->C[0] = _mm_add_ps(f->C[0], f->dC[0]);                                       // K1
   f->C[2] = _mm_add_ps(f->C[2], f->dC[2]);                                       // Q1
   __m128 g1 = _mm_add_ps(_mm_mul_ps(f->C[0], f2), _mm_mul_ps(f->C[2], f->R[0])); // K1*f2 + Q1*R0

   f->C[4] = _mm_add_ps(f->C[4], f->dC[4]); // V1
   f->C[5] = _mm_add_ps(f->C[5], f->dC[5]); // V2
   f->C[6] = _mm_add_ps(f->C[6], f->dC[6]); // V3
   __m128 y = _mm_add_ps(_mm_add_ps(_mm_mul_ps(f->C[6], g2), _mm_mul_ps(f->C[5], g1)),
                         _mm_mul_ps(f->C[4], f1));

   f->R[0] = _mm_mul_ps(f1, f->R[2]);
   f->R[1] = _mm_mul_ps(g1, f->R[2]);

   f->C[7] = _mm_add_ps(f->C[7], f->dC[7]); // Clipgain
   const __m128 m01 = _mm_set1_ps(0.1f);
   const __m128 m1 = _mm_set1_ps(1.0f);

   f->R[2] = _mm_max_ps(m01, _mm_sub_ps(m1, _mm_mul_ps(f->C[7], _mm_mul_ps(y, y))));

   return y;
}
#endif
#if PPC
#else
__m128 IIR12WDFquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   f->C[0] = _mm_add_ps(f->C[0], f->dC[0]); // E1 * sc
   f->C[1] = _mm_add_ps(f->C[1], f->dC[1]); // E2 * sc
   f->C[2] = _mm_add_ps(f->C[2], f->dC[2]); // -E1 / sc
   f->C[3] = _mm_add_ps(f->C[3], f->dC[3]); // -E2 / sc
   f->C[4] = _mm_add_ps(f->C[4], f->dC[4]); // C1
   f->C[5] = _mm_add_ps(f->C[5], f->dC[5]); // C2
   f->C[6] = _mm_add_ps(f->C[6], f->dC[6]); // D

   __m128 y = _mm_add_ps(_mm_add_ps(_mm_mul_ps(f->C[4], f->R[0]), _mm_mul_ps(f->C[6], in)),
                         _mm_mul_ps(f->C[5], f->R[1]));
   __m128 t =
       _mm_add_ps(in, _mm_add_ps(_mm_mul_ps(f->C[2], f->R[0]), _mm_mul_ps(f->C[3], f->R[1])));

   __m128 s1 = _mm_add_ps(_mm_mul_ps(t, f->C[0]), f->R[0]);
   __m128 s2 = _mm_sub_ps(_mm_setzero_ps(), _mm_add_ps(_mm_mul_ps(t, f->C[1]), f->R[1]));

   // f->R[0] = s1;
   // f->R[1] = s2;

   f->R[0] = _mm_mul_ps(s1, f->R[2]);
   f->R[1] = _mm_mul_ps(s2, f->R[2]);

   f->C[7] = _mm_add_ps(f->C[7], f->dC[7]); // Clipgain
   const __m128 m01 = _mm_set1_ps(0.1f);
   const __m128 m1 = _mm_set1_ps(1.0f);
   f->R[2] = _mm_max_ps(m01, _mm_sub_ps(m1, _mm_mul_ps(f->C[7], _mm_mul_ps(y, y))));

   return y;
}
#endif
#if PPC
__m128 IIR12CFCquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   // State-space with clipgain (2nd order, limit within register)

   f->C[0] = vec_add(f->C[0], f->dC[0]); // ar
   f->C[1] = vec_add(f->C[1], f->dC[1]); // ai
   f->C[2] = vec_add(f->C[2], f->dC[2]); // b1
   f->C[4] = vec_add(f->C[4], f->dC[4]); // c1
   f->C[5] = vec_add(f->C[5], f->dC[5]); // c2
   f->C[6] = vec_add(f->C[6], f->dC[6]); // d

   vFloat y = vec_madd(f->C[4], f->R[0], vec_madd(f->C[6], in, vec_madd(f->C[5], f->R[1], m0)));
   vFloat s1 = vec_madd(in, f->C[2], vec_madd(f->C[0], f->R[0], vec_nmsub(f->C[1], f->R[1], m0)));
   vFloat s2 = vec_madd(f->C[1], f->R[0], vec_madd(f->C[0], f->R[1], m0));

   f->R[0] = vec_madd(s1, f->R[2], m0);
   f->R[1] = vec_madd(s2, f->R[2], m0);

   f->C[7] = vec_add(f->C[7], f->dC[7]); // Clipgain
   const vFloat m01 = (vFloat)(0.1f);
   const vFloat m1 = (vFloat)(1.0f);
   f->R[2] = vec_max(m01, vec_nmsub(f->C[7], vec_madd(y, y, m0), m1));

   return y;
}
#else
__m128 IIR12CFCquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   // State-space with clipgain (2nd order, limit within register)

   f->C[0] = _mm_add_ps(f->C[0], f->dC[0]); // ar
   f->C[1] = _mm_add_ps(f->C[1], f->dC[1]); // ai
   f->C[2] = _mm_add_ps(f->C[2], f->dC[2]); // b1
   f->C[4] = _mm_add_ps(f->C[4], f->dC[4]); // c1
   f->C[5] = _mm_add_ps(f->C[5], f->dC[5]); // c2
   f->C[6] = _mm_add_ps(f->C[6], f->dC[6]); // d

   // y(i) = c1.*s(1) + c2.*s(2) + d.*x(i);
   // s1 = ar.*s(1) - ai.*s(2) + x(i);
   // s2 = ai.*s(1) + ar.*s(2);

   __m128 y = _mm_add_ps(_mm_add_ps(_mm_mul_ps(f->C[4], f->R[0]), _mm_mul_ps(f->C[6], in)),
                         _mm_mul_ps(f->C[5], f->R[1]));
   __m128 s1 = _mm_add_ps(_mm_mul_ps(in, f->C[2]),
                          _mm_sub_ps(_mm_mul_ps(f->C[0], f->R[0]), _mm_mul_ps(f->C[1], f->R[1])));
   __m128 s2 = _mm_add_ps(_mm_mul_ps(f->C[1], f->R[0]), _mm_mul_ps(f->C[0], f->R[1]));

   f->R[0] = _mm_mul_ps(s1, f->R[2]);
   f->R[1] = _mm_mul_ps(s2, f->R[2]);

   f->C[7] = _mm_add_ps(f->C[7], f->dC[7]); // Clipgain
   const __m128 m01 = _mm_set1_ps(0.1f);
   const __m128 m1 = _mm_set1_ps(1.0f);
   f->R[2] = _mm_max_ps(m01, _mm_sub_ps(m1, _mm_mul_ps(f->C[7], _mm_mul_ps(y, y))));

   return y;
}
#endif
#if PPC
#else
__m128 IIR12CFLquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   // State-space with softer limiter

   f->C[0] = _mm_add_ps(f->C[0], f->dC[0]); // (ar)
   f->C[1] = _mm_add_ps(f->C[1], f->dC[1]); // (ai)
   f->C[2] = _mm_add_ps(f->C[2], f->dC[2]); // b1
   f->C[4] = _mm_add_ps(f->C[4], f->dC[4]); // c1
   f->C[5] = _mm_add_ps(f->C[5], f->dC[5]); // c2
   f->C[6] = _mm_add_ps(f->C[6], f->dC[6]); // d

   // y(i) = c1.*s(1) + c2.*s(2) + d.*x(i);
   // s1 = ar.*s(1) - ai.*s(2) + x(i);
   // s2 = ai.*s(1) + ar.*s(2);

   __m128 y = _mm_add_ps(_mm_add_ps(_mm_mul_ps(f->C[4], f->R[0]), _mm_mul_ps(f->C[6], in)),
                         _mm_mul_ps(f->C[5], f->R[1]));
   __m128 ar = _mm_mul_ps(f->C[0], f->R[2]);
   __m128 ai = _mm_mul_ps(f->C[1], f->R[2]);
   __m128 s1 = _mm_add_ps(_mm_mul_ps(in, f->C[2]),
                          _mm_sub_ps(_mm_mul_ps(ar, f->R[0]), _mm_mul_ps(ai, f->R[1])));
   __m128 s2 = _mm_add_ps(_mm_mul_ps(ai, f->R[0]), _mm_mul_ps(ar, f->R[1]));

   f->R[0] = s1;
   f->R[1] = s2;

   /*m = 1 ./ max(1,abs(y(i)));
   mr = mr.*0.99 + m.*0.01;*/

   // Limiter
   const __m128 m001 = _mm_set1_ps(0.001f);
   const __m128 m099 = _mm_set1_ps(0.999f);
   const __m128 m1 = _mm_set1_ps(1.0f);
   const __m128 m2 = _mm_set1_ps(2.0f);

   __m128 m = _mm_rsqrt_ps(_mm_max_ps(m1, _mm_mul_ps(m2, _mm_and_ps(y, m128_mask_absval))));
   f->R[2] = _mm_add_ps(_mm_mul_ps(f->R[2], m099), _mm_mul_ps(m, m001));

   return y;
}
#endif
#if PPC
__m128 IIR24CFCquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   // State-space with clipgain (2nd order, limit within register)

   f->C[0] = vec_add(f->C[0], f->dC[0]); // ar
   f->C[1] = vec_add(f->C[1], f->dC[1]); // ai
   f->C[2] = vec_add(f->C[2], f->dC[2]); // b1
   f->C[4] = vec_add(f->C[4], f->dC[4]); // c1
   f->C[5] = vec_add(f->C[5], f->dC[5]); // c2
   f->C[6] = vec_add(f->C[6], f->dC[6]); // d

   vFloat y = vec_madd(f->C[4], f->R[0], vec_madd(f->C[6], in, vec_madd(f->C[5], f->R[1], m0)));
   vFloat s1 = vec_madd(in, f->C[2], vec_madd(f->C[0], f->R[0], vec_nmsub(f->C[1], f->R[1], m0)));
   vFloat s2 = vec_madd(f->C[1], f->R[0], vec_madd(f->C[0], f->R[1], m0));

   f->R[0] = vec_madd(s1, f->R[2], m0);
   f->R[1] = vec_madd(s2, f->R[2], m0);

   vFloat y2 = vec_madd(f->C[4], f->R[3], vec_madd(f->C[6], y, vec_madd(f->C[5], f->R[4], m0)));
   vFloat s3 = vec_madd(in, f->C[2], vec_madd(f->C[0], f->R[3], vec_nmsub(f->C[1], f->R[4], m0)));
   vFloat s4 = vec_madd(f->C[1], f->R[3], vec_madd(f->C[0], f->R[4], m0));

   f->R[3] = vec_madd(s3, f->R[2], m0);
   f->R[4] = vec_madd(s4, f->R[2], m0);

   f->C[7] = vec_add(f->C[7], f->dC[7]); // Clipgain
   const vFloat m01 = (vFloat)(0.1f);
   const vFloat m1 = (vFloat)(1.0f);
   f->R[2] = vec_max(m01, vec_nmsub(f->C[7], vec_madd(y2, y2, m0), m1));

   return y;
}
#else
__m128 IIR24CFCquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   // State-space with clipgain (2nd order, limit within register)

   f->C[0] = _mm_add_ps(f->C[0], f->dC[0]); // ar
   f->C[1] = _mm_add_ps(f->C[1], f->dC[1]); // ai
   f->C[2] = _mm_add_ps(f->C[2], f->dC[2]); // b1

   f->C[4] = _mm_add_ps(f->C[4], f->dC[4]); // c1
   f->C[5] = _mm_add_ps(f->C[5], f->dC[5]); // c2
   f->C[6] = _mm_add_ps(f->C[6], f->dC[6]); // d

   __m128 y = _mm_add_ps(_mm_add_ps(_mm_mul_ps(f->C[4], f->R[0]), _mm_mul_ps(f->C[6], in)),
                         _mm_mul_ps(f->C[5], f->R[1]));
   __m128 s1 = _mm_add_ps(_mm_mul_ps(in, f->C[2]),
                          _mm_sub_ps(_mm_mul_ps(f->C[0], f->R[0]), _mm_mul_ps(f->C[1], f->R[1])));
   __m128 s2 = _mm_add_ps(_mm_mul_ps(f->C[1], f->R[0]), _mm_mul_ps(f->C[0], f->R[1]));

   f->R[0] = _mm_mul_ps(s1, f->R[2]);
   f->R[1] = _mm_mul_ps(s2, f->R[2]);

   __m128 y2 = _mm_add_ps(_mm_add_ps(_mm_mul_ps(f->C[4], f->R[3]), _mm_mul_ps(f->C[6], y)),
                          _mm_mul_ps(f->C[5], f->R[4]));
   __m128 s3 = _mm_add_ps(_mm_mul_ps(y, f->C[2]),
                          _mm_sub_ps(_mm_mul_ps(f->C[0], f->R[3]), _mm_mul_ps(f->C[1], f->R[4])));
   __m128 s4 = _mm_add_ps(_mm_mul_ps(f->C[1], f->R[3]), _mm_mul_ps(f->C[0], f->R[4]));

   f->R[3] = _mm_mul_ps(s3, f->R[2]);
   f->R[4] = _mm_mul_ps(s4, f->R[2]);

   f->C[7] = _mm_add_ps(f->C[7], f->dC[7]); // Clipgain
   const __m128 m01 = _mm_set1_ps(0.1f);
   const __m128 m1 = _mm_set1_ps(1.0f);
   f->R[2] = _mm_max_ps(m01, _mm_sub_ps(m1, _mm_mul_ps(f->C[7], _mm_mul_ps(y2, y2))));

   return y2;
}
#endif
#if PPC
#else
__m128 IIR24CFLquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   // State-space with softer limiter

   f->C[0] = _mm_add_ps(f->C[0], f->dC[0]); // (ar)
   f->C[1] = _mm_add_ps(f->C[1], f->dC[1]); // (ai)
   f->C[2] = _mm_add_ps(f->C[2], f->dC[2]); // b1
   f->C[4] = _mm_add_ps(f->C[4], f->dC[4]); // c1
   f->C[5] = _mm_add_ps(f->C[5], f->dC[5]); // c2
   f->C[6] = _mm_add_ps(f->C[6], f->dC[6]); // d

   __m128 ar = _mm_mul_ps(f->C[0], f->R[2]);
   __m128 ai = _mm_mul_ps(f->C[1], f->R[2]);

   __m128 y = _mm_add_ps(_mm_add_ps(_mm_mul_ps(f->C[4], f->R[0]), _mm_mul_ps(f->C[6], in)),
                         _mm_mul_ps(f->C[5], f->R[1]));
   __m128 s1 = _mm_add_ps(_mm_mul_ps(in, f->C[2]),
                          _mm_sub_ps(_mm_mul_ps(ar, f->R[0]), _mm_mul_ps(ai, f->R[1])));
   __m128 s2 = _mm_add_ps(_mm_mul_ps(ai, f->R[0]), _mm_mul_ps(ar, f->R[1]));

   f->R[0] = s1;
   f->R[1] = s2;

   __m128 y2 = _mm_add_ps(_mm_add_ps(_mm_mul_ps(f->C[4], f->R[3]), _mm_mul_ps(f->C[6], y)),
                          _mm_mul_ps(f->C[5], f->R[4]));
   __m128 s3 = _mm_add_ps(_mm_mul_ps(y, f->C[2]),
                          _mm_sub_ps(_mm_mul_ps(ar, f->R[3]), _mm_mul_ps(ai, f->R[4])));
   __m128 s4 = _mm_add_ps(_mm_mul_ps(ai, f->R[3]), _mm_mul_ps(ar, f->R[4]));

   f->R[3] = s3;
   f->R[4] = s4;

   /*m = 1 ./ max(1,abs(y(i)));
   mr = mr.*0.99 + m.*0.01;*/

   // Limiter
   const __m128 m001 = _mm_set1_ps(0.001f);
   const __m128 m099 = _mm_set1_ps(0.999f);
   const __m128 m1 = _mm_set1_ps(1.0f);
   const __m128 m2 = _mm_set1_ps(2.0f);

   __m128 m = _mm_rsqrt_ps(_mm_max_ps(m1, _mm_mul_ps(m2, _mm_and_ps(y2, m128_mask_absval))));
   f->R[2] = _mm_add_ps(_mm_mul_ps(f->R[2], m099), _mm_mul_ps(m, m001));

   return y2;
}
#endif
#if PPC
__m128 IIR24Bquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   f->C[1] = vec_add(f->C[1], f->dC[1]); // K2
   f->C[3] = vec_add(f->C[3], f->dC[3]); // Q2
   f->C[0] = vec_add(f->C[0], f->dC[0]); // K1
   f->C[2] = vec_add(f->C[2], f->dC[2]); // Q1
   f->C[4] = vec_add(f->C[4], f->dC[4]); // V1
   f->C[5] = vec_add(f->C[5], f->dC[5]); // V2
   f->C[6] = vec_add(f->C[6], f->dC[6]); // V3

   __m128 f2 = vec_nmsub(f->C[1], f->R[1], vec_madd(f->C[3], in, m0)); // Q2*in - K2*R1
   __m128 g2 = vec_madd(f->C[3], f->R[1], vec_madd(f->C[1], in, m0));  // K2*in + Q2*R1
   __m128 f1 = vec_nmsub(f->C[0], f->R[0], vec_madd(f->C[2], f2, m0)); // Q1*f2 - K1*R0
   __m128 g1 = vec_madd(f->C[2], f->R[0], vec_madd(f->C[0], f2, m0));  // K1*f2 + Q1*R0
   f->R[0] = vec_madd(f1, f->R[4], m0);
   f->R[1] = vec_madd(g1, f->R[4], m0);

   vFloat y1 = vec_madd(f->C[6], g2, m0);
   y1 = vec_madd(f->C[5], g1, y1);
   y1 = vec_madd(f->C[4], f1, y1);

   f2 = vec_nmsub(f->C[1], f->R[3], vec_madd(f->C[3], y1, m0)); // Q2*in - K2*R1
   g2 = vec_madd(f->C[3], f->R[3], vec_madd(f->C[1], y1, m0));  // K2*in + Q2*R1
   f1 = vec_nmsub(f->C[0], f->R[2], vec_madd(f->C[2], f2, m0)); // Q1*f2 - K1*R0
   g1 = vec_madd(f->C[2], f->R[2], vec_madd(f->C[0], f2, m0));  // K1*f2 + Q1*R0

   f->R[2] = vec_madd(f1, f->R[4], m0);
   f->R[3] = vec_madd(g1, f->R[4], m0);

   vFloat y2 = vec_madd(f->C[6], g2, m0);
   y2 = vec_madd(f->C[5], g1, y1);
   y2 = vec_madd(f->C[4], f1, y1);

   f->C[7] = vec_add(f->C[7], f->dC[7]); // Clipgain
   const vFloat m01 = (vFloat)(0.1f);
   const vFloat m1 = (vFloat)(1.0f);
   f->R[4] = vec_max(m01, vec_nmsub(f->C[7], vec_madd(y2, y2, m0), m1));

   return y2;
}
#else
__m128 IIR24Bquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   f->C[1] = _mm_add_ps(f->C[1], f->dC[1]); // K2
   f->C[3] = _mm_add_ps(f->C[3], f->dC[3]); // Q2
   f->C[0] = _mm_add_ps(f->C[0], f->dC[0]); // K1
   f->C[2] = _mm_add_ps(f->C[2], f->dC[2]); // Q1
   f->C[4] = _mm_add_ps(f->C[4], f->dC[4]); // V1
   f->C[5] = _mm_add_ps(f->C[5], f->dC[5]); // V2
   f->C[6] = _mm_add_ps(f->C[6], f->dC[6]); // V3

   __m128 f2 = _mm_sub_ps(_mm_mul_ps(f->C[3], in), _mm_mul_ps(f->C[1], f->R[1])); // Q2*in - K2*R1
   __m128 g2 = _mm_add_ps(_mm_mul_ps(f->C[1], in), _mm_mul_ps(f->C[3], f->R[1])); // K2*in + Q2*R1
   __m128 f1 = _mm_sub_ps(_mm_mul_ps(f->C[2], f2), _mm_mul_ps(f->C[0], f->R[0])); // Q1*f2 - K1*R0
   __m128 g1 = _mm_add_ps(_mm_mul_ps(f->C[0], f2), _mm_mul_ps(f->C[2], f->R[0])); // K1*f2 + Q1*R0
   f->R[0] = _mm_mul_ps(f1, f->R[4]);
   f->R[1] = _mm_mul_ps(g1, f->R[4]);
   __m128 y1 = _mm_add_ps(_mm_add_ps(_mm_mul_ps(f->C[6], g2), _mm_mul_ps(f->C[5], g1)),
                          _mm_mul_ps(f->C[4], f1));

   f2 = _mm_sub_ps(_mm_mul_ps(f->C[3], y1), _mm_mul_ps(f->C[1], f->R[3])); // Q2*in - K2*R1
   g2 = _mm_add_ps(_mm_mul_ps(f->C[1], y1), _mm_mul_ps(f->C[3], f->R[3])); // K2*in + Q2*R1
   f1 = _mm_sub_ps(_mm_mul_ps(f->C[2], f2), _mm_mul_ps(f->C[0], f->R[2])); // Q1*f2 - K1*R0
   g1 = _mm_add_ps(_mm_mul_ps(f->C[0], f2), _mm_mul_ps(f->C[2], f->R[2])); // K1*f2 + Q1*R0
   f->R[2] = _mm_mul_ps(f1, f->R[4]);
   f->R[3] = _mm_mul_ps(g1, f->R[4]);
   __m128 y2 = _mm_add_ps(_mm_add_ps(_mm_mul_ps(f->C[6], g2), _mm_mul_ps(f->C[5], g1)),
                          _mm_mul_ps(f->C[4], f1));

   f->C[7] = _mm_add_ps(f->C[7], f->dC[7]); // Clipgain
   const __m128 m01 = _mm_set1_ps(0.1f);
   const __m128 m1 = _mm_set1_ps(1.0f);
   f->R[4] = _mm_max_ps(m01, _mm_sub_ps(m1, _mm_mul_ps(f->C[7], _mm_mul_ps(y2, y2))));

   return y2;
}
#endif
#if PPC
__m128 LPMOOGquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   const vFloat zero = (vFloat)0.f;
   f->C[0] = vec_add(f->C[0], f->dC[0]);
   f->C[1] = vec_add(f->C[1], f->dC[1]);
   f->C[2] = vec_add(f->C[2], f->dC[2]);

   f->R[0] =
       vec_softclip8(vec_madd(f->C[1],
                              vec_sub(vec_sub(vec_madd(in, f->C[0], zero),
                                              vec_madd(f->C[2], vec_add(f->R[3], f->R[4]), zero)),
                                      f->R[0]),
                              f->R[0]));

   f->R[1] = vec_madd(f->C[1], vec_sub(f->R[0], f->R[1]), f->R[1]);
   f->R[2] = vec_madd(f->C[1], vec_sub(f->R[1], f->R[2]), f->R[2]);
   f->R[4] = f->R[3];
   f->R[3] = vec_madd(f->C[1], vec_sub(f->R[2], f->R[3]), f->R[3]);

   return f->R[f->WP[0] & 3];
}
#else
__m128 LPMOOGquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   f->C[0] = _mm_add_ps(f->C[0], f->dC[0]);
   f->C[1] = _mm_add_ps(f->C[1], f->dC[1]);
   f->C[2] = _mm_add_ps(f->C[2], f->dC[2]);

   f->R[0] = softclip8_ps(_mm_add_ps(
       f->R[0],
       _mm_mul_ps(f->C[1], _mm_sub_ps(_mm_sub_ps(_mm_mul_ps(in, f->C[0]),
                                                 _mm_mul_ps(f->C[2], _mm_add_ps(f->R[3], f->R[4]))),
                                      f->R[0]))));
   f->R[1] = _mm_add_ps(f->R[1], _mm_mul_ps(f->C[1], _mm_sub_ps(f->R[0], f->R[1])));
   f->R[2] = _mm_add_ps(f->R[2], _mm_mul_ps(f->C[1], _mm_sub_ps(f->R[1], f->R[2])));
   f->R[4] = f->R[3];
   f->R[3] = _mm_add_ps(f->R[3], _mm_mul_ps(f->C[1], _mm_sub_ps(f->R[2], f->R[3])));

   return f->R[f->WP[0] & 3];
}
#endif
#if PPC
__m128 SNHquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   f->C[0] = vec_add(f->C[0], f->dC[0]);
   f->C[1] = vec_add(f->C[1], f->dC[1]);

   f->R[0] = vec_add(f->R[0], f->C[0]);

   vector bool mask = vec_cmpgt(f->R[0], m0);

   f->R[1] = vec_sel(f->R[1], vec_softclip(vec_nmsub(f->C[1], f->R[1], in)), mask);

   const vFloat m1 = (vFloat)(-1.f);
   f->R[0] = vec_add(f->R[0], vec_and(m1, mask));

   return f->R[1];
}
#else
__m128 SNHquad(QuadFilterUnitState* __restrict f, __m128 in)
{
   f->C[0] = _mm_add_ps(f->C[0], f->dC[0]);
   f->C[1] = _mm_add_ps(f->C[1], f->dC[1]);

   f->R[0] = _mm_add_ps(f->R[0], f->C[0]);

   __m128 mask = _mm_cmpgt_ps(f->R[0], _mm_setzero_ps());

   f->R[1] = _mm_or_ps(_mm_andnot_ps(mask, f->R[1]),
                       _mm_and_ps(mask, softclip_ps(_mm_sub_ps(in, _mm_mul_ps(f->C[1], f->R[1])))));

   const __m128 m1 = _mm_set1_ps(-1.f);
   f->R[0] = _mm_add_ps(f->R[0], _mm_and_ps(m1, mask));

   return f->R[1];
}
#endif
#if PPC
__m128 COMBquad_PPC(QuadFilterUnitState* __restrict f, __m128 in)
{
   const vFloat m256 = (vFloat)(256.f);
   const vSInt32 m0xff = (vSInt32)(0xff);

   f->C[0] = vec_add(f->C[0], f->dC[0]);
   f->C[1] = vec_add(f->C[1], f->dC[1]);

   vFloat a = vec_madd(f->C[0], m256, m0);
   vSInt32 e = vec_cts(a, 0);
   _MM_ALIGN16 int DTi[4], SEi[4];
   vSInt32 DT = vec_sr(e, vec_splat_s32(8));
   vec_st(DT, 0, DTi);
   vSInt32 SE = vec_and(e, m0xff);
   SE = vec_sub(m0xff, SE);
   vec_st(SE, 0, SEi);
   _MM_ALIGN16 float DBRead[4];

   for (int i = 0; i < 4; i++)
   {
      if (f->active[i])
      {
         int RP = (f->WP[i] - DTi[i] - FIRoffset) & (max_fb_comb - 1);

         // SINC interpolation (12 samples)
         float* lda = &f->DB[i][RP];
         vUInt8 mask = vec_lvsl(0, lda);
         vFloat a0 = vec_ld(0, lda);
         vFloat a1 = vec_ld(16, lda);
         vFloat a2 = vec_ld(32, lda);
         vFloat a3 = vec_ld(47, lda);
         vFloat a01 = vec_perm(a0, a1, mask);
         vFloat a12 = vec_perm(a1, a2, mask);
         vFloat a23 = vec_perm(a2, a3, mask);

         SEi[i] *= (FIRipol_N << 1);
         float* ldb = &sinctable[SEi[i]];
         vFloat o = vec_madd(a01, vec_ld(0, ldb), m0);
         o = vec_madd(a12, vec_ld(16, ldb), o);
         o = vec_madd(a23, vec_ld(32, ldb), o);

         float sum[4];
         vec_st(o, 0, sum);
         DBRead[i] = sum[0] + sum[1] + sum[2] + sum[3];
      }
   }

   vFloat DBR = vec_ld(0, DBRead);
   vFloat d = vec_madd(DBR, f->C[1], in);
   d = vec_softclip(d);

   for (int i = 0; i < 4; i++)
   {
      if (f->active[i])
      {
         // Write to delaybuffer (with "anti-wrapping")
         float t = *((float*)&d + i);
         f->DB[i][f->WP[i]] = t;
         if (f->WP[i] < FIRipol_N)
            f->DB[i][f->WP[i] + max_fb_comb] = t;

         // Increment write position
         f->WP[i] = (f->WP[i] + 1) & (max_fb_comb - 1);
      }
   }
   return vec_madd(f->C[3], DBR, vec_madd(f->C[2], in, m0));
}
#else
#if !_M_X64
__m128 COMBquad_SSE1(QuadFilterUnitState* __restrict f, __m128 in)
{
   assert(FIRipol_M == 256); // changing the constant requires updating the code below
   const __m128 m256 = _mm_set1_ps(256.f);
   const __m128i m0xff = _mm_set1_epi32(0xff);

   f->C[0] = _mm_add_ps(f->C[0], f->dC[0]);
   f->C[1] = _mm_add_ps(f->C[1], f->dC[1]);

   __m128 a = _mm_mul_ps(f->C[0], m256);
   __m128 DBRead = _mm_setzero_ps();

   for (int i = 0; i < 4; i++)
   {
      if (f->active[i])
      {
         int e = _mm_cvtss_si32(_mm_load_ss((float*)&a + i));
         int DT = e >> 8;
         int SE = (0xff - (e & 0xff)) * (FIRipol_N << 1);

         int RP = (f->WP[i] - DT - FIRoffset) & (max_fb_comb - 1);

         // SINC interpolation (12 samples)
         __m128 a = _mm_loadu_ps(&f->DB[i][RP]);
         __m128 b = _mm_load_ps(&sinctable[SE]);
         __m128 o = _mm_mul_ps(a, b);

         a = _mm_loadu_ps(&f->DB[i][RP + 4]);
         b = _mm_load_ps(&sinctable[SE + 4]);
         o = _mm_add_ps(o, _mm_mul_ps(a, b));

         a = _mm_loadu_ps(&f->DB[i][RP + 8]);
         b = _mm_load_ps(&sinctable[SE + 8]);
         o = _mm_add_ps(o, _mm_mul_ps(a, b));

         _mm_store_ss((float*)&DBRead + i, sum_ps_to_ss(o));
      }
   }

   __m128 d = _mm_add_ps(in, _mm_mul_ps(DBRead, f->C[1]));
   d = softclip_ps(d);

   for (int i = 0; i < 4; i++)
   {
      if (f->active[i])
      {
         // Write to delaybuffer (with "anti-wrapping")
         __m128 t = _mm_load_ss((float*)&d + i);
         _mm_store_ss(&f->DB[i][f->WP[i]], t);
         if (f->WP[i] < FIRipol_N)
            _mm_store_ss(&f->DB[i][f->WP[i] + max_fb_comb], t);

         // Increment write position
         f->WP[i] = (f->WP[i] + 1) & (max_fb_comb - 1);
      }
   }
   return _mm_add_ps(_mm_mul_ps(f->C[3], DBRead), _mm_mul_ps(f->C[2], in));
}
#endif

__m128 COMBquad_SSE2(QuadFilterUnitState* __restrict f, __m128 in)
{
   assert(FIRipol_M == 256); // changing the constant requires updating the code below
   const __m128 m256 = _mm_set1_ps(256.f);
   const __m128i m0xff = _mm_set1_epi32(0xff);

   f->C[0] = _mm_add_ps(f->C[0], f->dC[0]);
   f->C[1] = _mm_add_ps(f->C[1], f->dC[1]);

   __m128 a = _mm_mul_ps(f->C[0], m256);
   __m128i e = _mm_cvtps_epi32(a);
   _MM_ALIGN16 int DTi[4], SEi[4];
   __m128i DT = _mm_srli_epi32(e, 8);
   _mm_store_si128((__m128i*)DTi, DT);
   __m128i SE = _mm_and_si128(e, m0xff);
   SE = _mm_sub_epi32(m0xff, SE);
   _mm_store_si128((__m128i*)SEi, SE);
   __m128 DBRead = _mm_setzero_ps();

   for (int i = 0; i < 4; i++)
   {
      if (f->active[i])
      {
         int RP = (f->WP[i] - DTi[i] - FIRoffset) & (max_fb_comb - 1);

         // SINC interpolation (12 samples)
         __m128 a = _mm_loadu_ps(&f->DB[i][RP]);
         SEi[i] *= (FIRipol_N << 1);
         __m128 b = _mm_load_ps(&sinctable[SEi[i]]);
         __m128 o = _mm_mul_ps(a, b);

         a = _mm_loadu_ps(&f->DB[i][RP + 4]);
         b = _mm_load_ps(&sinctable[SEi[i] + 4]);
         o = _mm_add_ps(o, _mm_mul_ps(a, b));

         a = _mm_loadu_ps(&f->DB[i][RP + 8]);
         b = _mm_load_ps(&sinctable[SEi[i] + 8]);
         o = _mm_add_ps(o, _mm_mul_ps(a, b));

         _mm_store_ss((float*)&DBRead + i, sum_ps_to_ss(o));
      }
   }

   __m128 d = _mm_add_ps(in, _mm_mul_ps(DBRead, f->C[1]));
   d = softclip_ps(d);

   for (int i = 0; i < 4; i++)
   {
      if (f->active[i])
      {
         // Write to delaybuffer (with "anti-wrapping")
         __m128 t = _mm_load_ss((float*)&d + i);
         _mm_store_ss(&f->DB[i][f->WP[i]], t);
         if (f->WP[i] < FIRipol_N)
            _mm_store_ss(&f->DB[i][f->WP[i] + max_fb_comb], t);

         // Increment write position
         f->WP[i] = (f->WP[i] + 1) & (max_fb_comb - 1);
      }
   }
   return _mm_add_ps(_mm_mul_ps(f->C[3], DBRead), _mm_mul_ps(f->C[2], in));
}
#endif
#if PPC
vFloat CLIP(vFloat in, vFloat drive)
{
   const vFloat x_min = (vFloat)(-1.0f);
   const vFloat x_max = (vFloat)(1.0f);
   return vec_max(vec_min(vec_madd(in, drive, m0), x_max), x_min);
}
#else
__m128 CLIP(__m128 in, __m128 drive)
{
   const __m128 x_min = _mm_set1_ps(-1.0f);
   const __m128 x_max = _mm_set1_ps(1.0f);
   return _mm_max_ps(_mm_min_ps(_mm_mul_ps(in, drive), x_max), x_min);
}
#endif
#if PPC
vFloat DIGI_PPC(vFloat in, vFloat drive)
{
   const vFloat mofs = (vFloat)(0.5f);
   const vFloat m16 = (vFloat)(16.f);
   const vFloat m16inv = (vFloat)(0.0625f);
   const vFloat mneg32inv = (vFloat)(-0.5f * 0.0625f);

   vFloat invdrive = vec_re(drive);
   vFloat a = vec_madd(invdrive, vec_madd(m16, in, m0), mofs);
   return vec_madd(drive, vec_madd(m16inv, vec_trunc(a), mneg32inv), m0);
}
#else
__m128 DIGI_SSE2(__m128 in, __m128 drive)
{
   // v1.2: return (double)((int)((double)(x*p0inv*16.f+1.0)))*p0*0.0625f;
   const __m128 m16 = _mm_set1_ps(16.f);
   const __m128 m16inv = _mm_set1_ps(0.0625f);
   const __m128 mofs = _mm_set1_ps(0.5f);

   __m128 invdrive = _mm_rcp_ps(drive);
   __m128i a = _mm_cvtps_epi32(_mm_add_ps(mofs, _mm_mul_ps(invdrive, _mm_mul_ps(m16, in))));

   return _mm_mul_ps(drive, _mm_mul_ps(m16inv, _mm_sub_ps(_mm_cvtepi32_ps(a), mofs)));
}

#if !_M_X64
__m128 DIGI_SSE1(__m128 in, __m128 drive)
{
   const __m128 mofs = _mm_set1_ps(0.5f);
   const __m128 m16 = _mm_set1_ps(16.f);
   const __m128 m16inv = _mm_set1_ps(0.0625f);

   __m128 invdrive = _mm_rcp_ps(drive);
   __m128 a = _mm_add_ps(mofs, _mm_mul_ps(invdrive, _mm_mul_ps(m16, in)));
   __m64 a1 = _mm_cvtps_pi32(a);
   __m64 a2 = _mm_cvtps_pi32(_mm_movehl_ps(a, a));
   a = _mm_cvtpi32_ps(_mm_movelh_ps(a, _mm_cvtpi32_ps(a, a2)), a1);
   __m128 b = _mm_mul_ps(drive, _mm_mul_ps(m16inv, _mm_sub_ps(a, mofs)));
   _mm_empty();
   return b;
}
#endif

#endif
#if PPC
__m128 TANH(__m128 in, __m128 drive)
{
   const vFloat m9 = (vFloat)(9.f);
   const vFloat m27 = (vFloat)(27.f);

   vFloat x = vec_madd(in, drive, m0);
   vFloat xx = vec_madd(x, x, m0);
   vFloat denom = vec_madd(m9, xx, m27);
   vFloat y = vec_madd(x, vec_add(m27, xx), m0);
   y = vec_madd(y, vec_re(denom), m0);

   const vFloat y_min = (vFloat)(-1.0f);
   const vFloat y_max = (vFloat)(1.0f);
   return vec_max(vec_min(y, y_max), y_min);
}
#else
__m128 TANH(__m128 in, __m128 drive)
{
   // Closer to ideal than TANH0
   // y = x * ( 27 + x * x ) / ( 27 + 9 * x * x );
   // y = clip(y)

   const __m128 m9 = _mm_set1_ps(9.f);
   const __m128 m27 = _mm_set1_ps(27.f);

   __m128 x = _mm_mul_ps(in, drive);
   __m128 xx = _mm_mul_ps(x, x);
   __m128 denom = _mm_add_ps(m27, _mm_mul_ps(m9, xx));
   __m128 y = _mm_mul_ps(x, _mm_add_ps(m27, xx));
   y = _mm_mul_ps(y, _mm_rcp_ps(denom));

   const __m128 y_min = _mm_set1_ps(-1.0f);
   const __m128 y_max = _mm_set1_ps(1.0f);
   return _mm_max_ps(_mm_min_ps(y, y_max), y_min);
}
#endif
#if PPC
__m128 SINUS_PPC(__m128 in, __m128 drive)
{
   const vFloat m256 = (vFloat)(256.f);
   const vFloat m512 = (vFloat)(512.f);

   vFloat x = vec_madd(in, drive, m0);
   x = vec_madd(x, m256, m512);
   vSInt32Union e;
   e.V = vec_cts(x, 0);
   vFloat a = vec_sub(x, vec_trunc(x));

   // funkar inte med optimization ?

   _MM_ALIGN16 float ws4[4], wsn4[4];
   ws4[0] = waveshapers[3][e.Int[0] & 0x3ff];
   ws4[1] = waveshapers[3][e.Int[1] & 0x3ff];
   ws4[2] = waveshapers[3][e.Int[2] & 0x3ff];
   ws4[3] = waveshapers[3][e.Int[3] & 0x3ff];
   wsn4[0] = waveshapers[3][(e.Int[0] + 1) & 0x3ff];
   wsn4[1] = waveshapers[3][(e.Int[1] + 1) & 0x3ff];
   wsn4[2] = waveshapers[3][(e.Int[2] + 1) & 0x3ff];
   wsn4[3] = waveshapers[3][(e.Int[3] + 1) & 0x3ff];

   vFloat ws = vec_ld(0, ws4);
   vFloat wsn = vec_ld(0, wsn4);

   // x = vec_madd(vec_sub(one,a),ws,vec_madd(a,wsn,m0));
   x = vec_madd(a, wsn, vec_nmsub(a, ws, ws));
   return x;
}
#else
#if !_M_X64
__m128 SINUS_SSE1(__m128 in, __m128 drive)
{
   const __m128 one = _mm_set1_ps(1.f);
   const __m128 m256 = _mm_set1_ps(256.f);
   const __m128 m512 = _mm_set1_ps(512.f);

   __m128 x = _mm_mul_ps(in, drive);
   x = _mm_add_ps(_mm_mul_ps(x, m256), m512);

   __m64 e1 = _mm_cvtps_pi32(x);
   __m64 e2 = _mm_cvtps_pi32(_mm_movehl_ps(x, x));
   __m128 a = _mm_sub_ps(x, _mm_cvtpi32_ps(_mm_movelh_ps(x, _mm_cvtpi32_ps(x, e2)), e1));
   int e11 = *(int*)&e1;
   int e12 = *((int*)&e1 + 1);
   int e21 = *(int*)&e2;
   int e22 = *((int*)&e2 + 1);

   __m128 ws1 = _mm_load_ss(&waveshapers[3][e11 & 0x3ff]);
   __m128 ws2 = _mm_load_ss(&waveshapers[3][e12 & 0x3ff]);
   __m128 ws3 = _mm_load_ss(&waveshapers[3][e21 & 0x3ff]);
   __m128 ws4 = _mm_load_ss(&waveshapers[3][e22 & 0x3ff]);
   __m128 ws = _mm_movelh_ps(_mm_unpacklo_ps(ws1, ws2), _mm_unpacklo_ps(ws3, ws4));
   ws1 = _mm_load_ss(&waveshapers[3][(e11 + 1) & 0x3ff]);
   ws2 = _mm_load_ss(&waveshapers[3][(e12 + 1) & 0x3ff]);
   ws3 = _mm_load_ss(&waveshapers[3][(e21 + 1) & 0x3ff]);
   ws4 = _mm_load_ss(&waveshapers[3][(e22 + 1) & 0x3ff]);
   __m128 wsn = _mm_movelh_ps(_mm_unpacklo_ps(ws1, ws2), _mm_unpacklo_ps(ws3, ws4));

   x = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(one, a), ws), _mm_mul_ps(a, wsn));

   _mm_empty();

   return x;
}
#endif
__m128 SINUS_SSE2(__m128 in, __m128 drive)
{
   const __m128 one = _mm_set1_ps(1.f);
   const __m128 m256 = _mm_set1_ps(256.f);
   const __m128 m512 = _mm_set1_ps(512.f);

   __m128 x = _mm_mul_ps(in, drive);
   x = _mm_add_ps(_mm_mul_ps(x, m256), m512);

   __m128i e = _mm_cvtps_epi32(x);
   __m128 a = _mm_sub_ps(x, _mm_cvtepi32_ps(e));
   e = _mm_packs_epi32(e, e);

#if MAC
   // this should be very fast on C2D/C1D (and there are no macs with K8's)
   // GCC seems to optimize around the XMM -> int transfers so this is needed here
   _MM_ALIGN16 int e4[4];
   e4[0] = _mm_cvtsi128_si32(e);
   e4[1] = _mm_cvtsi128_si32(_mm_shufflelo_epi16(e, _MM_SHUFFLE(1, 1, 1, 1)));
   e4[2] = _mm_cvtsi128_si32(_mm_shufflelo_epi16(e, _MM_SHUFFLE(2, 2, 2, 2)));
   e4[3] = _mm_cvtsi128_si32(_mm_shufflelo_epi16(e, _MM_SHUFFLE(3, 3, 3, 3)));
#else
   // on PC write to memory & back as XMM -> GPR is slow on K8
   _MM_ALIGN16 short e4[8];
   _mm_store_si128((__m128i*)&e4, e);
#endif

   __m128 ws1 = _mm_load_ss(&waveshapers[3][e4[0] & 0x3ff]);
   __m128 ws2 = _mm_load_ss(&waveshapers[3][e4[1] & 0x3ff]);
   __m128 ws3 = _mm_load_ss(&waveshapers[3][e4[2] & 0x3ff]);
   __m128 ws4 = _mm_load_ss(&waveshapers[3][e4[3] & 0x3ff]);
   __m128 ws = _mm_movelh_ps(_mm_unpacklo_ps(ws1, ws2), _mm_unpacklo_ps(ws3, ws4));
   ws1 = _mm_load_ss(&waveshapers[3][(e4[0] + 1) & 0x3ff]);
   ws2 = _mm_load_ss(&waveshapers[3][(e4[1] + 1) & 0x3ff]);
   ws3 = _mm_load_ss(&waveshapers[3][(e4[2] + 1) & 0x3ff]);
   ws4 = _mm_load_ss(&waveshapers[3][(e4[3] + 1) & 0x3ff]);
   __m128 wsn = _mm_movelh_ps(_mm_unpacklo_ps(ws1, ws2), _mm_unpacklo_ps(ws3, ws4));

   x = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(one, a), ws), _mm_mul_ps(a, wsn));

   return x;
}
#endif
#if PPC
__m128 ASYM_PPC(__m128 in, __m128 drive)
{
   const vFloat m32 = (vFloat)(32.f);
   const vFloat m512 = (vFloat)(512.f);
   const vSInt32 LB = (vSInt32)(0);
   const vSInt32 UB = (vSInt32)(0x3fe);

   vFloat x = vec_madd(in, drive, m0);
   x = vec_madd(x, m32, m512);
   vSInt32Union e;
   e.V = vec_cts(x, 0);
   vFloat a = vec_sub(x, vec_trunc(x));
   e.V = vec_max(vec_min(e.V, UB), LB);

   // Does not work with optimization?

   _MM_ALIGN16 float ws4[4], wsn4[4];
   ws4[0] = waveshapers[2][e.Int[0] & 0x3ff];
   ws4[1] = waveshapers[2][e.Int[1] & 0x3ff];
   ws4[2] = waveshapers[2][e.Int[2] & 0x3ff];
   ws4[3] = waveshapers[2][e.Int[3] & 0x3ff];
   wsn4[0] = waveshapers[2][(e.Int[0] + 1) & 0x3ff];
   wsn4[1] = waveshapers[2][(e.Int[1] + 1) & 0x3ff];
   wsn4[2] = waveshapers[2][(e.Int[2] + 1) & 0x3ff];
   wsn4[3] = waveshapers[2][(e.Int[3] + 1) & 0x3ff];

   vFloat ws = vec_ld(0, ws4);
   vFloat wsn = vec_ld(0, wsn4);

   // x = vec_madd(vec_sub(one,a),ws,vec_madd(a,wsn,m0));
   x = vec_madd(a, wsn, vec_nmsub(a, ws, ws));
   return x;
}
#else
#if !_M_X64
__m128 ASYM_SSE1(__m128 in, __m128 drive)
{
   const __m128 one = _mm_set1_ps(1.f);
   const __m128 m32 = _mm_set1_ps(32.f);
   const __m128 m512 = _mm_set1_ps(512.f);
   const __m64 LB = _mm_set1_pi16(0);
   const __m64 UB = _mm_set1_pi16(0x3fe);

   __m128 x = _mm_mul_ps(in, drive);
   x = _mm_add_ps(_mm_mul_ps(x, m32), m512);

   __m64 e1 = _mm_cvtps_pi32(x);
   __m64 e2 = _mm_cvtps_pi32(_mm_movehl_ps(x, x));
   __m128 a = _mm_sub_ps(x, _mm_cvtpi32_ps(_mm_movelh_ps(x, _mm_cvtpi32_ps(x, e2)), e1));

   e1 = _mm_packs_pi32(e1, e2);
   e1 = _mm_max_pi16(_mm_min_pi16(e1, UB), LB);

   int e11 = *(int*)&e1;
   int e12 = *((int*)&e1 + 1);
   int e21 = *(int*)&e2;
   int e22 = *((int*)&e2 + 1);

   __m128 ws1 = _mm_load_ss(&waveshapers[2][e11 & 0x3ff]);
   __m128 ws2 = _mm_load_ss(&waveshapers[2][e12 & 0x3ff]);
   __m128 ws3 = _mm_load_ss(&waveshapers[2][e21 & 0x3ff]);
   __m128 ws4 = _mm_load_ss(&waveshapers[2][e22 & 0x3ff]);
   __m128 ws = _mm_movelh_ps(_mm_unpacklo_ps(ws1, ws2), _mm_unpacklo_ps(ws3, ws4));
   ws1 = _mm_load_ss(&waveshapers[2][(e11 + 1) & 0x3ff]);
   ws2 = _mm_load_ss(&waveshapers[2][(e12 + 1) & 0x3ff]);
   ws3 = _mm_load_ss(&waveshapers[2][(e21 + 1) & 0x3ff]);
   ws4 = _mm_load_ss(&waveshapers[2][(e22 + 1) & 0x3ff]);
   __m128 wsn = _mm_movelh_ps(_mm_unpacklo_ps(ws1, ws2), _mm_unpacklo_ps(ws3, ws4));

   x = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(one, a), ws), _mm_mul_ps(a, wsn));

   _mm_empty();

   return x;
}
#endif

__m128 ASYM_SSE2(__m128 in, __m128 drive)
{
   const __m128 one = _mm_set1_ps(1.f);
   const __m128 m32 = _mm_set1_ps(32.f);
   const __m128 m512 = _mm_set1_ps(512.f);
   const __m128i UB = _mm_set1_epi16(0x3fe);

   __m128 x = _mm_mul_ps(in, drive);
   x = _mm_add_ps(_mm_mul_ps(x, m32), m512);

   __m128i e = _mm_cvtps_epi32(x);
   __m128 a = _mm_sub_ps(x, _mm_cvtepi32_ps(e));
   e = _mm_packs_epi32(e, e);
   e = _mm_max_epi16(_mm_min_epi16(e, UB), _mm_setzero_si128());

#if MAC
   // this should be very fast on C2D/C1D (and there are no macs with K8's)
   _MM_ALIGN16 int e4[4];
   e4[0] = _mm_cvtsi128_si32(e);
   e4[1] = _mm_cvtsi128_si32(_mm_shufflelo_epi16(e, _MM_SHUFFLE(1, 1, 1, 1)));
   e4[2] = _mm_cvtsi128_si32(_mm_shufflelo_epi16(e, _MM_SHUFFLE(2, 2, 2, 2)));
   e4[3] = _mm_cvtsi128_si32(_mm_shufflelo_epi16(e, _MM_SHUFFLE(3, 3, 3, 3)));

#else
   // on PC write to memory & back as XMM -> GPR is slow on K8
   _MM_ALIGN16 short e4[8];
   _mm_store_si128((__m128i*)&e4, e);
#endif

   __m128 ws1 = _mm_load_ss(&waveshapers[2][e4[0] & 0x3ff]);
   __m128 ws2 = _mm_load_ss(&waveshapers[2][e4[1] & 0x3ff]);
   __m128 ws3 = _mm_load_ss(&waveshapers[2][e4[2] & 0x3ff]);
   __m128 ws4 = _mm_load_ss(&waveshapers[2][e4[3] & 0x3ff]);
   __m128 ws = _mm_movelh_ps(_mm_unpacklo_ps(ws1, ws2), _mm_unpacklo_ps(ws3, ws4));
   ws1 = _mm_load_ss(&waveshapers[2][(e4[0] + 1) & 0x3ff]);
   ws2 = _mm_load_ss(&waveshapers[2][(e4[1] + 1) & 0x3ff]);
   ws3 = _mm_load_ss(&waveshapers[2][(e4[2] + 1) & 0x3ff]);
   ws4 = _mm_load_ss(&waveshapers[2][(e4[3] + 1) & 0x3ff]);
   __m128 wsn = _mm_movelh_ps(_mm_unpacklo_ps(ws1, ws2), _mm_unpacklo_ps(ws3, ws4));

   x = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(one, a), ws), _mm_mul_ps(a, wsn));

   return x;
}
#endif

FilterUnitQFPtr GetQFPtrFilterUnit(int type, int subtype)
{
   switch (type)
   {
   case fut_lp12:
   {
      if (subtype == st_SVF)
         return SVFLP12Aquad;
      else if (subtype == st_Rough)
         return IIR12CFCquad;
      //			else if(subtype==st_Medium)
      //				return IIR12CFLquad;
      return IIR12Bquad;
   }
   case fut_hp12:
   {
      if (subtype == st_SVF)
         return SVFHP12Aquad;
      else if (subtype == st_Rough)
         return IIR12CFCquad;
      //			else if(subtype==st_Medium)
      //				return IIR12CFLquad;
      return IIR12Bquad;
   }
   case fut_bp12:
   {
      if (subtype == st_SVF)
         return SVFBP12Aquad;
      else if (subtype == 3)
         return SVFBP24Aquad;
      else if (subtype == st_Rough)
         return IIR12CFCquad;
      //			else if(subtype==st_Medium)
      //				return SVFBP24Aquad;
      return IIR12Bquad;
   }
   case fut_br12:
      return IIR12Bquad;
   case fut_lp24:
      if (subtype == st_SVF)
         return SVFLP24Aquad;
      else if (subtype == st_Rough)
         return IIR24CFCquad;
      //		else if(subtype==st_Medium)
      //			return IIR24CFLquad;
      return IIR24Bquad;
   case fut_hp24:
      if (subtype == st_SVF)
         return SVFHP24Aquad;
      else if (subtype == st_Rough)
         return IIR24CFCquad;
      //		else if(subtype==st_Medium)
      //			return IIR24CFLquad;
      return IIR24Bquad;
   case fut_lpmoog:
      return LPMOOGquad;
   case fut_SNH:
      return SNHquad;
   case fut_comb:
#if PPC
      return COMBquad_PPC;
#else
#if !_M_X64
      if (!(CpuArchitecture & CaSSE2))
         return COMBquad_SSE1;
#endif
      return COMBquad_SSE2;
#endif
   }
   return 0;
}

WaveshaperQFPtr GetQFPtrWaveshaper(int type)
{
#if PPC
   switch (type)
   {
   case wst_digi:
      return DIGI_PPC;
   case wst_hard:
      return CLIP;
   case wst_sinus:
      return SINUS_PPC;
   case wst_asym:
      return ASYM_PPC;
   case wst_tanh:
      return TANH;
   }
#else
   switch (type)
   {
   case wst_digi:
#if !_M_X64
      if (!(CpuArchitecture & CaSSE2))
         return DIGI_SSE1;
#endif
      return DIGI_SSE2;
   case wst_hard:
      return CLIP;
   case wst_sinus:
#if !_M_X64
      if (!(CpuArchitecture & CaSSE2))
         return SINUS_SSE1;
#endif
      return SINUS_SSE2;
   case wst_asym:
#if !_M_X64
      if (!(CpuArchitecture & CaSSE2))
         return ASYM_SSE1;
#endif
      return ASYM_SSE2;
   case wst_tanh:
      return TANH;
   }
#endif
   return 0;
}
