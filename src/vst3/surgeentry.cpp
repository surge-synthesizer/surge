#include "version.h" // for versioning
#include "public.sdk/source/main/pluginfactoryvst3.h"
#include "SurgeVst3Processor.h"
//#include "SurgeVst3EditController.h"
#include "surgecids.h"
#include "CpuArchitecture.h"
#include <AbstractSynthesizer.h>

#define stringPluginName "Surge"

//------------------------------------------------------------------------
//  Module init/exit
//------------------------------------------------------------------------

//------------------------------------------------------------------------
// called after library was loaded
bool InitModule()
{
   initCpuArchitecture();

   return true;
}

//------------------------------------------------------------------------
// called after library is unloaded
bool DeinitModule()
{
   return true;
}

//------------------------------------------------------------------------
//  VST Plug-in Entry
//------------------------------------------------------------------------
// Windows: do not forget to include a .def file in your project to export
// GetPluginFactory function!
//------------------------------------------------------------------------

BEGIN_FACTORY_DEF("Vember Audio", "http://www.vemberaudio.se", "mailto:info@vemberaudio.se")

//---First Plug-in included in this factory-------
// its kVstAudioEffectClass component
DEF_CLASS2(INLINE_UID_FROM_FUID(SurgeProcessorUID),
           PClassInfo::kManyInstances, // cardinality
           kVstAudioEffectClass,       // the component category (dont change this)
           stringPluginName,           // here the Plug-in name (to be changed)
           0, // Vst::kDistributable,	// means that component and controller could be distributed
              // on different computers
           "Instrument|Synth", // Subcategory for this Plug-in (to be changed)
           FULL_VERSION_STR,   // Plug-in version (to be changed)
           kVstVersionString,  // the VST 3 SDK version (dont changed this, use always this define)
           SurgeVst3Processor::createInstance) // function pointer called when this component should
                                               // be instanciated

/*// its kVstComponentControllerClass component
DEF_CLASS2 (INLINE_UID_FROM_FUID (SurgeControllerUID),
PClassInfo::kManyInstances,  // cardinality
kVstComponentControllerClass,// the Controller category (dont change this)
stringPluginName "Controller",	// controller name (could be the same than component name)
0,						// not used here
"",						// not used here
FULL_VERSION_STR,		// Plug-in version (to be changed)
kVstVersionString,		// the VST 3 SDK version (dont changed this, use always this define)
SurgeController::createInstance)// function pointer called when this component should be
instanciated*/

//----for others Plug-ins contained in this factory, put like for the first Plug-in different
// DEF_CLASS2---

END_FACTORY
