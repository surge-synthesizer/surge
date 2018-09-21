#define MAC 1
#define MACX 1
#define WINDOWS 0
#define SGI 0
#define MOTIF 0
#define BEBOX 0
#if __ppc__
#define PPC 1
#endif

#if PPC && !AU
#define VST_FORCE_DEPRECATED 0
#endif

#define USE_NAMESPACE 1

#define TARGET_API_MAC_CARBON 1
#define CARBON 1
#define USENAVSERVICES 1

#define __CF_USE_FRAMEWORK_INCLUDES__

#if __MWERKS__
#define __NOEXTENSIONS__
#define MAC_OS_X_VERSION_MIN_REQUIRED 1020
#define MAC_OS_X_VERSION_MAX_ALLOWED 1040
#include <AvailabilityMacros.h>
#endif

#define QUARTZ 1

#include <Carbon/Carbon.h>
#include <Accelerate/Accelerate.h>
#include <vstgui/vstgui.h>
#include "globals.h"

#define _MM_ALIGN16 __attribute__((aligned(16)))
#define __forceinline inline
#define stricmp strcmp
#define _aligned_malloc(x, y) malloc(x)
#define _aligned_free(x) free(x)

#if PPC
#define __m128 vFloat
#else
#define SSE_VERSION 3
#include <emmintrin.h>
#endif
