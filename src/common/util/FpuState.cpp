#include "globals.h"
#include "FpuState.h"
#ifndef ARM_NEON
#include <emmintrin.h>
#endif
#include <float.h>

FpuState::FpuState() : _old_SSE_state(0), _SSE_Flags(0x8040)
{}

void FpuState::set()
{
#ifndef ARM_NEON
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
#endif
}

void FpuState::restore()
{
#ifndef ARM_NEON
   _mm_setcsr(_old_SSE_state);
#endif
}
