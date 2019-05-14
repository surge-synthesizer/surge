#include "FpuState.h"
#include "SIMD.h"
#include <float.h>

FpuState::FpuState() : _old_SSE_state(0), _SSE_Flags(0x8040)
{}

void FpuState::set()
{
   bool fpuExceptions = false;

   _old_SSE_state = _mm_getcsr();
   if (fpuExceptions)
   {
      _mm_setcsr(((_old_SSE_state & ~_MM_MASK_MASK) | _SSE_Flags) | _MM_EXCEPT_MASK); // all on
   }
   else
   {
      _mm_setcsr((_old_SSE_state | _SSE_Flags) | _MM_MASK_MASK);
   }
   // FTZ/DAZ + ignore all exceptions (1 means ignored)

   _MM_SET_ROUNDING_MODE(_MM_ROUND_NEAREST);
}

void FpuState::restore()
{
   _mm_setcsr(_old_SSE_state);
}
