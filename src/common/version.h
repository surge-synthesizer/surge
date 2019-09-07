#ifndef __version__
#define __version__

#include "pluginterfaces/base/fplatform.h"

#define SURGE_VST2_IDENTIFIER 'cjs3'

#define MAJOR_VERSION_STR "1"
#define MAJOR_VERSION_INT 1

#define SUB_VERSION_STR "6"
#define SUB_VERSION_INT 6

#define RELEASE_NUMBER_STR "2"
#define RELEASE_NUMBER_INT 2

#define BUILD_NUMBER_STR "100" // Build number to be sure that each result could identified.
#define BUILD_NUMBER_INT 100

// Version with build number (example "1.0.3.342")
#define FULL_VERSION_STR																							\
   MAJOR_VERSION_STR "." SUB_VERSION_STR "." RELEASE_NUMBER_STR "." BUILD_NUMBER_STR

// Version without build number (example "1.0.3")
#define VERSION_STR MAJOR_VERSION_STR "." SUB_VERSION_STR "." RELEASE_NUMBER_STR

#define stringOriginalFilename "surge.vst3"
#if PLATFORM_64
#define stringFileDescription "Vember Audio Surge"
#else
#define stringFileDescription "Vember Audio Surge"
#endif

#define stringCompanyName "Vember Audio\0"
#define stringLegalCopyright "© 2017 Vember Audio"
#define stringLegalTrademarks "VST is a trademark of Steinberg Media Technologies GmbH"

#endif //__version__
