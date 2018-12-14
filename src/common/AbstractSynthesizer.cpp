//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & vember|audio
//-------------------------------------------------------------------------------------------------------
#include "AbstractSynthesizer.h"
#if WINDOWS
#include <Windows.h>
#endif

#include <CpuArchitecture.h>

const char *SSE2ErrorText =
   "This plugin requires a CPU supporting the SSE2 instruction set.";
const char *CMOVErrorText =
   "This plugin requires a CPU supporting the CMOV instruction.";

void initDllGlobals()
{
   initCpuArchitecture();

   if (!(CpuArchitecture & CaSSE2))
   {
#if WINDOWS
      MessageBox(::GetActiveWindow(), SSE2ErrorText,
                 "Surge: System requirements not met", MB_OK | MB_ICONERROR);
#else
      fprintf(stderr, "%s: %s", __func__, SSE2ErrorText);
#endif
   }
   if (!(CpuArchitecture & CaCMOV))
   {
#if WINDOWS
      MessageBox(::GetActiveWindow(), CMOVErrorText,
                 "Surge: System requirements not met", MB_OK | MB_ICONERROR);
#else
      fprintf(stderr, "%s: %s", __func__, CMOVErrorText);
#endif
   }
}
