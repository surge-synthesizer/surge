#pragma once

struct QuadFilterUnitState;
class FilterCoefficientMaker;
class SurgeStorage;

namespace NonlinearStatesFilter
{
void makeCoefficients(FilterCoefficientMaker *cm, float freq, float reso, int subtype,
                      SurgeStorage *storage);
__m128 process(QuadFilterUnitState *__restrict f, __m128 in);
} // namespace NonlinearStatesFilter
