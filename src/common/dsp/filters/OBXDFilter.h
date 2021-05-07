#ifndef obxd_h
#define obxd_h

#include "QuadFilterUnit.h"

namespace ObxdFilter
{
enum Poles
{
    TWO_POLE,
    FOUR_POLE
};
void makeCoefficients(FilterCoefficientMaker *cm, Poles p, float freq, float reso, int sub,
                      SurgeStorage *storage);
__m128 process_2_pole(QuadFilterUnitState *__restrict f, __m128 sample);
__m128 process_4_pole(QuadFilterUnitState *__restrict f, __m128 sample);
} // namespace ObxdFilter

#endif
