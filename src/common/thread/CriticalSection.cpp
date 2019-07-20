#include "CriticalSection.h"

#if WINDOWS

#include "assert.h"

namespace Surge {
    
CriticalSection::CriticalSection()
{
   refcount = 0;
   InitializeCriticalSection(&cs);
}

CriticalSection::~CriticalSection()
{
   DeleteCriticalSection(&cs);
}

void CriticalSection::enter()
{
   EnterCriticalSection(&cs);
   refcount++;
   assert(refcount > 0);
   assert(!(refcount > 10)); // if its >10 there's some crazy *§%* going on ^^
}

void CriticalSection::leave()
{
   refcount--;
   if (refcount < 0)
   {
      int breakpointme = 0;
   }
   assert(refcount >= 0);
   LeaveCriticalSection(&cs);
}

}
#endif
