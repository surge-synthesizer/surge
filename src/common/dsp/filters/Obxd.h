#ifndef obxd_h
#define obxd_h

#include "QuadFilterUnit.h"

namespace ObxdFilter {
   void makeCoefficients(FilterCoefficientMaker *cm, float freq, float reso, int sub, SurgeStorage* storage);
   __m128 process_2_pole(QuadFilterUnitState * __restrict f, __m128 sample);
   __m128 process_4_pole(QuadFilterUnitState * __restrict f, __m128 sample);
}

#endif
