#pragma once

#if MAC
#include <CoreServices/CoreServices.h>
#else
#include "windows.h"
#endif

class c_sec
{
public:
	c_sec();
	~c_sec();
	void enter();
	void leave();
protected:
	#if MAC
	MPCriticalRegionID cs;
	#else
	CRITICAL_SECTION cs;
	#endif
	int refcount;
};