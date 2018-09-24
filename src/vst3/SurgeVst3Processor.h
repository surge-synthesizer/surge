#pragma once

#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/vst/ivstevents.h"
#include <util/FpuState.h>
#include <memory>

using namespace Steinberg;

class SurgeGUIEditor;
class SurgeSynthesizer;

// we need public EditController, public IAudioProcessor
class SurgeVst3Processor : public Steinberg::Vst::SingleComponentEffect //, public IMidiMapping
{
public:
   SurgeVst3Processor();
   virtual ~SurgeVst3Processor(); // dont forget virtual here

   //------------------------------------------------------------------------
   // create function required for Plug-in factory,
   // it will be called to create new instances of this Plug-in
   //------------------------------------------------------------------------
   static FUnknown* createInstance(void* context)
   {
      return (Steinberg::Vst::IAudioProcessor*)new SurgeVst3Processor;
   }

   //------------------------------------------------------------------------
   // AudioEffect overrides:
   //------------------------------------------------------------------------
   /** Called at first after constructor */
   tresult PLUGIN_API initialize(FUnknown* context);

   /** Called at the end before destructor */
   tresult PLUGIN_API terminate();

   /** Switch the Plug-in on/off */
   tresult PLUGIN_API setActive(TBool state);

   tresult PLUGIN_API setProcessing(TBool state);

   /** Here we go...the process call */
   tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data);

   /** Will be called before any process call */
   tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& newSetup);

   /** Bus arrangement managing: in this example the 'again' will be mono for mono input/output and
    * stereo for other arrangements. */
   tresult PLUGIN_API setBusArrangements(Steinberg::Vst::SpeakerArrangement* inputs,
                                         int32 numIns,
                                         Steinberg::Vst::SpeakerArrangement* outputs,
                                         int32 numOuts);

   // Controller part

   IPlugView* PLUGIN_API createView(const char* name);

   virtual void editorDestroyed(Steinberg::Vst::EditorView* editor)
   {} // nothing to do here
   virtual void editorAttached(Steinberg::Vst::EditorView* editor);
   virtual void editorRemoved(Steinberg::Vst::EditorView* editor);

   void addDependentView(SurgeGUIEditor* view);
   void removeDependentView(SurgeGUIEditor* view);

   // from IEditController
   tresult PLUGIN_API setState(IBStream* state);
   tresult PLUGIN_API getState(IBStream* state);
   virtual int32 PLUGIN_API getParameterCount();
   virtual tresult PLUGIN_API getParameterInfo(int32 paramIndex,
                                               Steinberg::Vst::ParameterInfo& info);
   virtual tresult PLUGIN_API getParamStringByValue(Steinberg::Vst::ParamID tag,
                                                    Steinberg::Vst::ParamValue valueNormalized,
                                                    Steinberg::Vst::String128 string);
   virtual tresult PLUGIN_API getParamValueByString(Steinberg::Vst::ParamID tag,
                                                    Steinberg::Vst::TChar* string,
                                                    Steinberg::Vst::ParamValue& valueNormalized);
   virtual Steinberg::Vst::ParamValue PLUGIN_API
   normalizedParamToPlain(Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue valueNormalized);
   virtual Steinberg::Vst::ParamValue PLUGIN_API
   plainParamToNormalized(Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue plainValue);
   virtual Steinberg::Vst::ParamValue PLUGIN_API getParamNormalized(Steinberg::Vst::ParamID tag);
   virtual tresult PLUGIN_API setParamNormalized(Steinberg::Vst::ParamID tag,
                                                 Steinberg::Vst::ParamValue value);

   void processEvents(int sampleOffset, Steinberg::Vst::IEventList* noteEvents, int& eventIndex);
   void processEvent(const Steinberg::Vst::Event& e);

   void processParameterChanges(int sampleOffset,
                                Steinberg::Vst::IParameterChanges* parameterEvents,
                                int& eventIndex);

   SurgeSynthesizer* getSurge();

   virtual tresult PLUGIN_API
   getMidiControllerAssignment(int32 busIndex,
                               int16 channel,
                               Steinberg::Vst::CtrlNumber midiControllerNumber,
                               Steinberg::Vst::ParamID& id /*out*/);

   //! when true, surge exports all normal 128 CC parameters, aftertouch and pitch bend as
   //! parameters (but not automatable)
   bool exportAllMidiControllers();

   void updateDisplay();
   void setParameterAutomated(int externalparam, float value);

protected:
   void createSurge();
   void destroySurge();

   bool isRegularController(int32 paramIndex);
   bool isMidiMapController(int32 paramIndex);

   int32 getParameterCountWithoutMappings();

   unique_ptr<SurgeSynthesizer> surgeInstance;
   std::vector<SurgeGUIEditor*> viewsArray;
   int blockpos;

   FpuState _fpuState;
};
