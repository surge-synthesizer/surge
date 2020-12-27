#pragma once

#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include <util/FpuState.h>
#include <memory>
#include <set>
#include <map>

using namespace Steinberg;

class SurgeGUIEditor;
class SurgeSynthesizer;

namespace VSTGUI
{
	class CBitmap;
}

// we need public EditController, public IAudioProcessor
class SurgeVst3Processor : public Steinberg::Vst::SingleComponentEffect,
                           public Steinberg::Vst::IMidiMapping
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
   tresult PLUGIN_API initialize(FUnknown* context) override;

   /** Called at the end before destructor */
   tresult PLUGIN_API terminate() override;

   /** Switch the Plug-in on/off */
   tresult PLUGIN_API setActive(TBool state) override;

   tresult PLUGIN_API setProcessing(TBool state) override;

   /** Here we go...the process call */
   tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) override;

   /** Will be called before any process call */
   tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& newSetup) override;

   /** Bus arrangement managing: in this example the 'again' will be mono for mono input/output and
    * stereo for other arrangements. */
   tresult PLUGIN_API setBusArrangements(Steinberg::Vst::SpeakerArrangement* inputs,
                                         int32 numIns,
                                         Steinberg::Vst::SpeakerArrangement* outputs,
                                         int32 numOuts) override;

   // Controller part

   IPlugView* PLUGIN_API createView(const char* name) override;

   void editorDestroyed(Steinberg::Vst::EditorView* editor) override
   {} // nothing to do here
   void editorAttached(Steinberg::Vst::EditorView* editor) override;
   void editorRemoved(Steinberg::Vst::EditorView* editor) override;

   void addDependentView(SurgeGUIEditor* view);
   void removeDependentView(SurgeGUIEditor* view);

   // from IEditController
   tresult PLUGIN_API setState(IBStream* state) override;
   tresult PLUGIN_API getState(IBStream* state) override;
   int32 PLUGIN_API getParameterCount() override;
   tresult PLUGIN_API getParameterInfo(int32 paramIndex,
                                       Steinberg::Vst::ParameterInfo& info) override;
   tresult PLUGIN_API getParamStringByValue(Steinberg::Vst::ParamID tag,
                                            Steinberg::Vst::ParamValue valueNormalized,
                                            Steinberg::Vst::String128 string) override;
   tresult PLUGIN_API getParamValueByString(Steinberg::Vst::ParamID tag,
                                            Steinberg::Vst::TChar* string,
                                            Steinberg::Vst::ParamValue& valueNormalized) override;
   Steinberg::Vst::ParamValue PLUGIN_API normalizedParamToPlain(
       Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue valueNormalized) override;
   Steinberg::Vst::ParamValue PLUGIN_API plainParamToNormalized(
       Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue plainValue) override;
   Steinberg::Vst::ParamValue PLUGIN_API getParamNormalized(Steinberg::Vst::ParamID tag) override;
   tresult PLUGIN_API setParamNormalized(Steinberg::Vst::ParamID tag,
                                         Steinberg::Vst::ParamValue value) override;

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
                               Steinberg::Vst::ParamID& id /*out*/) override;

   void updateDisplay();
   void setParameterAutomated(int externalparam, float value);

   tresult beginEdit(Steinberg::Vst::ParamID id) override;
   tresult performEdit(Steinberg::Vst::ParamID id,
                       Steinberg::Vst::ParamValue valueNormalized) override;
   tresult endEdit(Steinberg::Vst::ParamID id) override;

   void uithreadIdleActivity();

protected:
   bool isFirstLoad = true;
   void setExtraScaleFactor(VSTGUI::CBitmap *bg, float zf);

   void createSurge();
   void destroySurge();

   bool isRegularController(int32 paramIndex);
   bool isMidiMapController(int32 paramIndex);

   int32 getParameterCountWithoutMappings();

   std::unique_ptr<SurgeSynthesizer> surgeInstance;
   std::set<SurgeGUIEditor*> viewsSet;
   std::map<int, int> beginEditGuard;
   int blockpos = 0;

   float lastZoom = -1;
   void resize(SurgeGUIEditor *e, int width, int heigth);
   void redraw(SurgeGUIEditor *e, bool resizeWindow);
   
   FpuState _fpuState;

   int midi_controller_0, midi_controller_max;
   const int n_midi_controller_params = 16 * (Steinberg::Vst::ControllerNumbers::kCountCtrlNumber);

   int checkNamesEvery = 0;
   
public:
   OBJ_METHODS(SurgeVst3Processor, Steinberg::Vst::SingleComponentEffect)
   DEFINE_INTERFACES
   DEF_INTERFACE(Steinberg::Vst::IMidiMapping)
   END_DEFINE_INTERFACES(Steinberg::Vst::SingleComponentEffect)
   REFCOUNT_METHODS(Steinberg::Vst::SingleComponentEffect)
};
