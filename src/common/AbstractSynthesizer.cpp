//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & vember|audio
//-------------------------------------------------------------------------------------------------------
#include "AbstractSynthesizer.h"

#define SSE_STATE_FLAG 0x8040

#if !MAC && !__linux__
#include <CpuArchitecture.h>

int SSE_VERSION;

#include <Windows.h>
#endif

void initDllGlobals()
{
#if !MAC && !__linux__ // intel macs always support SSE2
   unsigned int arch = determine_support();
   // detect
   if (arch & ca_SSE3)
   {
      SSE_VERSION = 3;
   }
   else if (arch & ca_SSE2)
   {
      SSE_VERSION = 2;
   }
   else
   {
      SSE_VERSION = 0;
      MessageBox(::GetActiveWindow(),
                 "This plugin requires a CPU supporting the SSE2 instruction set.",
                 "Surge: System requirements not met", MB_OK | MB_ICONERROR);
   }
   if (!(arch & ca_CMOV))
   {
      MessageBox(::GetActiveWindow(), "This plugin requires a CPU supporting the CMOV instruction.",
                 "Surge: System requirements not met", MB_OK | MB_ICONERROR);
   }
#endif
}
