#pragma once

#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/vst/ivstevents.h"
#include <util/FpuState.h>
#include <memory>

using namespace Steinberg;
using namespace Steinberg::Vst;

class sub3_editor;
class sub3_synth;

// we need public EditController, public IAudioProcessor
class SurgeProcessor : public SingleComponentEffect //, public IMidiMapping
{
public:
   SurgeProcessor();
   virtual ~SurgeProcessor(); // dont forget virtual here

   //------------------------------------------------------------------------
   // create function required for Plug-in factory,
   // it will be called to create new instances of this Plug-in
   //------------------------------------------------------------------------
   static FUnknown* createInstance(void* context)
   {
      return (IAudioProcessor*)new SurgeProcessor;
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
   tresult PLUGIN_API process(ProcessData& data);

   /** Will be called before any process call */
   tresult PLUGIN_API setupProcessing(ProcessSetup& newSetup);

   /** Bus arrangement managing: in this example the 'again' will be mono for mono input/output and
    * stereo for other arrangements. */
   tresult PLUGIN_API setBusArrangements(SpeakerArrangement* inputs,
                                         int32 numIns,
                                         SpeakerArrangement* outputs,
                                         int32 numOuts);

   // Controller part

   IPlugView* PLUGIN_API createView(const char* name);

   virtual void editorDestroyed(EditorView* editor)
   {} // nothing to do here
   virtual void editorAttached(EditorView* editor);
   virtual void editorRemoved(EditorView* editor);

   void addDependentView(sub3_editor* view);
   void removeDependentView(sub3_editor* view);

   // from IEditController
   tresult PLUGIN_API setState(IBStream* state);
   tresult PLUGIN_API getState(IBStream* state);
   virtual int32 PLUGIN_API getParameterCount();
   virtual tresult PLUGIN_API getParameterInfo(int32 paramIndex, ParameterInfo& info);
   virtual tresult PLUGIN_API getParamStringByValue(ParamID tag,
                                                    ParamValue valueNormalized,
                                                    String128 string);
   virtual tresult PLUGIN_API getParamValueByString(ParamID tag,
                                                    TChar* string,
                                                    ParamValue& valueNormalized);
   virtual ParamValue PLUGIN_API normalizedParamToPlain(ParamID tag, ParamValue valueNormalized);
   virtual ParamValue PLUGIN_API plainParamToNormalized(ParamID tag, ParamValue plainValue);
   virtual ParamValue PLUGIN_API getParamNormalized(ParamID tag);
   virtual tresult PLUGIN_API setParamNormalized(ParamID tag, ParamValue value);

   void processEvents(int sampleOffset, IEventList* noteEvents, int& eventIndex);
   void processEvent(const Event& e);

   void
   processParameterChanges(int sampleOffset, IParameterChanges* parameterEvents, int& eventIndex);

   sub3_synth* getSurge();

   virtual tresult PLUGIN_API getMidiControllerAssignment(int32 busIndex,
                                                          int16 channel,
                                                          CtrlNumber midiControllerNumber,
                                                          ParamID& id /*out*/);

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

   unique_ptr<sub3_synth> surgeInstance;
   std::vector<sub3_editor*> viewsArray;
   int blockpos;

   FpuState _fpuState;
};
