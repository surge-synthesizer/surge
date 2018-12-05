#include "CpuArchitecture.h"
#include <string.h>

unsigned int CpuArchitecture;

extern "C" void __cpuid(int* CPUInfo, int InfoType);
#pragma intrinsic(__cpuid)

void initCpuArchitecture()
{
#if WINDOWS
   int CPUInfo[4] = {-1};
   __cpuid(CPUInfo, 0);
   char vendor[12];
   memcpy(vendor, &CPUInfo[1], 4 * sizeof(char));
   memcpy(&vendor[8], &CPUInfo[2], 4 * sizeof(char));
   memcpy(&vendor[4], &CPUInfo[3], 4 * sizeof(char));
   if (!CPUInfo[0])
      return 0; // no additional instructions supported

   __cpuid(CPUInfo, 1);
   if ((1 << 15) & CPUInfo[3])
      CpuArchitecture |= CaCMOV;
   if ((1 << 26) & CPUInfo[3])
      CpuArchitecture |= CaSSE2;
#else
   __builtin_cpu_init();
   if (__builtin_cpu_supports("sse2"))
      CpuArchitecture |= CaSSE2;
   if (__builtin_cpu_supports("cmov"))
      CpuArchitecture |= CaCMOV;
#endif
}
