#pragma once

struct QuadFilterUnitState;
class FilterCoefficientMaker;
class SurgeStorage;

namespace DiodeLadderFilter 
{
   void makeCoefficients( FilterCoefficientMaker *cm, float freq, float reso, SurgeStorage *storage );
   __m128 process( QuadFilterUnitState * __restrict f, __m128 in );
}
