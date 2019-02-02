#pragma once

#if MAC
#include <CoreServices/CoreServices.h>
#elif LINUX
#else
#include "windows.h"
#endif

#if LINUX

#include <stdio.h>
#include <pthread.h>
#include <assert.h>

class CriticalSection
{
public:
  CriticalSection(){
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mutex, &attr);
  }
  
  ~CriticalSection(){
  }
  
  void enter(){
    pthread_mutex_lock(&mutex);
    refcount++;
    assert(refcount > 0);
    assert(!(refcount > 5)); // if its >5 there's some crazy *§%* going on ^^
  }
  void leave(){
    refcount--;
    assert(refcount >= 0);
    pthread_mutex_unlock(&mutex);
  }

protected:
  pthread_mutex_t mutex;
  int refcount = 0;
};

#else

class CriticalSection
{
public:
   CriticalSection();
   ~CriticalSection();
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

#endif

