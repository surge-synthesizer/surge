#pragma once

struct QuadFilterUnitState;
class FilterCoefficientMaker;
class SurgeStorage;

namespace NonlinearFeedbackFilter 
{
   void makeCoefficientsLPHP( FilterCoefficientMaker *cm, float freq, float reso, int subtype, SurgeStorage *storage );
   void makeCoefficientsNBP ( FilterCoefficientMaker *cm, float freq, float reso, int subtype, SurgeStorage *storage );
   __m128 process( QuadFilterUnitState * __restrict f, __m128 in );
}
