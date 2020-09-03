#pragma once

class QuadFilterUnitState;
class FilterCoefficientMaker;

namespace RKMoog
{
   void makeCoefficients( FilterCoefficientMaker *cm, float freq, float reso );
   __m128 process( QuadFilterUnitState * __restrict f, __m128 in );
}
