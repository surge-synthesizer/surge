#pragma once

struct QuadFilterUnitState;
class FilterCoefficientMaker;
class SurgeStorage;

namespace K35Filter
{
void makeCoefficients(FilterCoefficientMaker *cm, float freq, float reso, bool is_lowpass,
                      float saturation, SurgeStorage *storage);
__m128 process_lp(QuadFilterUnitState *__restrict f, __m128 in);
__m128 process_hp(QuadFilterUnitState *__restrict f, __m128 in);
} // namespace K35Filter
