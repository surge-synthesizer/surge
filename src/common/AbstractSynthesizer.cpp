//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & vember|audio
//-------------------------------------------------------------------------------------------------------
#include "AbstractSynthesizer.h"
#include "UserInteractions.h"

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
       Surge::UserInteractions::promptError( SSE2ErrorText, "Surge System Requirements not met" );
   }
   if (!(CpuArchitecture & CaCMOV))
   {
       Surge::UserInteractions::promptError( CMOVErrorText, "Surge System Requirements not met" );
   }
}
