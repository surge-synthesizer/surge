#pragma once

class QuadFilterUnitState;
class FilterCoefficientMaker;
class SurgeStorage;

namespace VintageLadder
{
   namespace RK {
      void makeCoefficients( FilterCoefficientMaker *cm, float freq, float reso, SurgeStorage *storage );
      __m128 process( QuadFilterUnitState * __restrict f, __m128 in );
   }

   namespace Huov {
      void makeCoefficients( FilterCoefficientMaker *cm, float freq, float reso, SurgeStorage *storage );
      __m128 process( QuadFilterUnitState * __restrict f, __m128 in );
   }

   namespace Kraj {
      inline void makeCoefficients( FilterCoefficientMaker *cm, float freq, float reso, SurgeStorage *storage ) {}
      inline __m128 process( QuadFilterUnitState * __restrict f, __m128 in ) { return _mm_setzero_ps(); }
   }

   namespace Stilson {
      inline void makeCoefficients( FilterCoefficientMaker *cm, float freq, float reso, SurgeStorage *storage ) {}
      inline __m128 process( QuadFilterUnitState * __restrict f, __m128 in ) { return _mm_setzero_ps(); }
   }

   namespace Improved {
      inline void makeCoefficients( FilterCoefficientMaker *cm, float freq, float reso, SurgeStorage *storage ) {}
      inline __m128 process( QuadFilterUnitState * __restrict f, __m128 in ) { return _mm_setzero_ps(); }
   }
}
