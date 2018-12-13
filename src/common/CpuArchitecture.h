#ifndef CPUARCHITECTURE_H
#define CPUARCHITECTURE_H

enum CpuArchitecture
{
   CaSSE2 = 0x2,
   CaCMOV = 0x40,
};

extern unsigned int CpuArchitecture;

void initCpuArchitecture();

#endif // CPUARCHITECTURE_H
