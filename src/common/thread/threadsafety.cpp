#include "threadsafety.h"
#include "assert.h"

c_sec::c_sec()
{
	refcount = 0;
#if MAC
	MPCreateCriticalRegion (&cs);
#else
	InitializeCriticalSection(&cs);
#endif
}

c_sec::~c_sec()
{
#if MAC
	MPDeleteCriticalRegion(cs);
#else
	DeleteCriticalSection(&cs);
#endif
}

void c_sec::enter()
{
#if MAC
	MPEnterCriticalRegion(cs,kDurationForever);
#else
	EnterCriticalSection(&cs);
#endif
	refcount++;
	assert(refcount>0);
	assert(!(refcount>5));	// if its >5 there's some crazy *§%* going on ^^	
}

void c_sec::leave()
{
	refcount--;
	if(refcount < 0)
	{
		int breakpointme=0;	
	}
	assert(refcount >= 0);	
#if MAC
	MPExitCriticalRegion(cs);
#else
	LeaveCriticalSection(&cs);
#endif
}