#pragma once

struct QuadFilterUnitState;
class FilterCoefficientMaker;
class SurgeStorage;

namespace NonlinearFeedbackFilter 
{
   void makeCoefficients( FilterCoefficientMaker *cm, float freq, float reso, int subtype, bool oversample, SurgeStorage *storage );
   __m128 process( QuadFilterUnitState * __restrict f, __m128 in );
   __m128 process_oversampled( QuadFilterUnitState * __restrict f, __m128 in );
}
