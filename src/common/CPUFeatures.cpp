/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2021 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include "CPUFeatures.h"
#include "globals.h"

#if MAC
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#if LINUX
#include <fstream>
#endif

#ifdef _MSC_VER
#include <intrin.h>
#endif

#ifdef _WIN32
//  Windows
#define cpuid(info, x) __cpuidex(info, x, 0)
#else
#if LINUX && !ARM_NEON
#ifdef __GNUC__
//  GCC Intrinsics
#include <cpuid.h>
void cpuid(int info[4], int InfoType)
{
    __cpuid_count(InfoType, 0, info[0], info[1], info[2], info[3]);
}
#endif
#endif
#endif

namespace Surge
{
namespace CPUFeatures
{

std::string cpuBrand()
{
    std::string arch = "Unknown CPU";
#if MAC

    char buffer[1024];
    size_t bufsz = sizeof(buffer);
    if (sysctlbyname("machdep.cpu.brand_string", (void *)(&buffer), &bufsz, nullptr, (size_t)0) < 0)
    {
#if ARM_NEON
        arch = "Apple Silicon";
#endif
    }
    else
    {
        arch = buffer;
#if ARM_NEON
        arch += " (Apple Silicon)";
#endif
    }

#elif WINDOWS
    std::string platform = "Windows";

    int CPUInfo[4] = {-1};
    unsigned nExIds, i = 0;
    char CPUBrandString[0x40];
    // Get the information associated with each extended ID.
    __cpuid(CPUInfo, 0x80000000);
    nExIds = CPUInfo[0];
    for (i = 0x80000000; i <= nExIds; ++i)
    {
        __cpuid(CPUInfo, i);
        // Interpret CPU brand string
        if (i == 0x80000002)
            memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000003)
            memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000004)
            memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
    }
    arch = CPUBrandString;
#elif LINUX
    std::string platform = "Linux";

    // Lets see what /proc/cpuinfo has to say for us
    // on intels this is "model name"
    auto pinfo = std::ifstream("/proc/cpuinfo");
    if (pinfo.is_open())
    {
        std::string line;
        while (std::getline(pinfo, line))
        {
            if (line.find("model name") == 0)
            {
                auto colon = line.find(":");
                arch = line.substr(colon + 1);
                break;
            }
            if (line.find("Model") == 0) // rasperry pi branch
            {
                auto colon = line.find(":");
                arch = line.substr(colon + 1);
                break;
            }
        }
    }
    pinfo.close();
#endif
    return arch;
}

bool isArm()
{
#if ARM_NEON
    return true;
#else
    return false;
#endif
}
bool isX86()
{
#if ARM_NEON
    return false;
#else
    return true;
#endif
}
bool hasSSE2() { return true; }
bool hasAVX()
{
#if ARM_NEON
    return true; // thanks simde
#else
#if MAC
    return true;
#endif
#if WINDOWS || LINUX
    int info[4];
    cpuid(info, 0);
    unsigned int nIds = info[0];

    cpuid(info, 0x80000000);
    unsigned nExIds = info[0];

    bool avxSup = false;
    if (nIds >= 0x00000001)
    {
        cpuid(info, 0x00000001);

        avxSup = (info[2] & ((int)1 << 28)) != 0;
    }

    return avxSup;
#endif

#endif
}

FPUStateGuard::FPUStateGuard()
{
#ifndef ARM_NEON
    auto _SSE_Flags = 0x8040;
    bool fpuExceptions = false;

    priorS = _mm_getcsr();
    if (fpuExceptions)
    {
        _mm_setcsr(((priorS & ~_MM_MASK_MASK) | _SSE_Flags) | _MM_EXCEPT_MASK); // all on
    }
    else
    {
        _mm_setcsr((priorS | _SSE_Flags) | _MM_MASK_MASK);
    }

    _MM_SET_ROUNDING_MODE(_MM_ROUND_NEAREST);
#endif
}

FPUStateGuard::~FPUStateGuard()
{
#ifndef ARM_NEON
    _mm_setcsr(priorS);
#endif
}
} // namespace CPUFeatures
} // namespace Surge
