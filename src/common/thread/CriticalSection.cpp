#include "CriticalSection.h"

#if !LINUX

#include "assert.h"

CriticalSection::CriticalSection()
{
   refcount = 0;
#if MAC
   MPCreateCriticalRegion(&cs);
#else
   InitializeCriticalSection(&cs);
#endif
}

CriticalSection::~CriticalSection()
{
#if MAC
   MPDeleteCriticalRegion(cs);
#else
   DeleteCriticalSection(&cs);
#endif
}

void CriticalSection::enter()
{
#if MAC
   MPEnterCriticalRegion(cs, kDurationForever);
#else
   EnterCriticalSection(&cs);
#endif
   refcount++;
   assert(refcount > 0);
   assert(!(refcount > 5)); // if its >5 there's some crazy *§%* going on ^^
}

void CriticalSection::leave()
{
   refcount--;
   if (refcount < 0)
   {
      int breakpointme = 0;
   }
   assert(refcount >= 0);
#if MAC
   MPExitCriticalRegion(cs);
#else
   LeaveCriticalSection(&cs);
#endif
}

#endif
