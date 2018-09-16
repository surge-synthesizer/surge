#if !MAC
#include "cpuarch.h"
#include <string.h>

extern "C"
void __cpuid(int* CPUInfo, int InfoType);
#pragma intrinsic(__cpuid)


unsigned int determine_support()
{
	unsigned int arch=0;
	int CPUInfo[4] = {-1};
	__cpuid(CPUInfo,0);
	char vendor[12];
	memcpy(vendor,&CPUInfo[1],4*sizeof(char));
	memcpy(&vendor[8],&CPUInfo[2],4*sizeof(char));
	memcpy(&vendor[4],&CPUInfo[3],4*sizeof(char));
	if(!CPUInfo[0]) return 0;	// no additional instructions supported

	__cpuid(CPUInfo,1);
	if ((1<<15)&CPUInfo[3]) arch |= ca_CMOV;
	if ((1<<25)&CPUInfo[3]) arch |= ca_SSE;
	if ((1<<26)&CPUInfo[3]) arch |= ca_SSE2;
	if (CPUInfo[2] & 0x1) arch |= ca_SSE3;

	// get number of extended pages
	__cpuid(CPUInfo,0x80000000);
	unsigned int extendedpages = CPUInfo[0];
	
	// determine 3DNow! support
	if (!strncmp("AuthenticAMD",vendor,12) && (extendedpages >= 0x80000001))
	{		
		__cpuid(CPUInfo,0x80000001);
		if (((1<<30)&CPUInfo[3])&&((1<<31)&CPUInfo[3])) arch |= ca_3DNOW;	
	}

	return arch;
}
#endif