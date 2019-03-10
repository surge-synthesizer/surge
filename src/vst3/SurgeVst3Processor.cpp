#include <gui/SurgeGUIEditor.h>

#include "SurgeVst3Processor.h"
#include "surgecids.h"
#include "UserInteractions.h"

#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/base/ustring.h"

#include "CScalableBitmap.h"

#include <algorithm>
#include <cwchar>
#include <codecvt>
#include <string.h>

#define MIDI_CONTROLLER_0 20000
#define MIDI_CONTROLLER_MAX 24096

using namespace Steinberg::Vst;

#define CHECK_INITIALIZED                                                                          \
   if (!surgeInstance.get())                                                                       \
   {                                                                                               \
      return kNotInitialized;                                                                      \
   }

#define ABORT_IF_NOT_INITIALIZED                                                                   \
   if (!surgeInstance.get())                                                                       \
   {                                                                                               \
      return 0;                                                                                    \
   }

SurgeVst3Processor::SurgeVst3Processor() : blockpos(0), surgeInstance()
{}

SurgeVst3Processor::~SurgeVst3Processor()
{
   destroySurge();
}

tresult PLUGIN_API SurgeVst3Processor::initialize(FUnknown* context)
{
   //---always initialize the parent-------
   tresult result = SingleComponentEffect::initialize(context);
   // if everything Ok, continue
   if (result != kResultOk)
   {
      return result;
   }

   //---create Audio In/Out busses------
   // we want a stereo Input and a Stereo Output
   addAudioInput(STR16("Stereo In"), SpeakerArr::kStereo);
   addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);

   //---create Event In/Out busses (1 bus with 16 channels)------
   addEventInput(USTRING("MIDI In"));

   // addUnit(new Unit(USTRING ("Macro Parameters"), 1));

   /*      case 1: return L"Macro Parameters";
         case 2: return L"Global / FX";
         case 3: return L"Scene A Common";
         case 4: return L"Scene A Osc";
         case 5: return L"Scene A Osc Mixer";
         case 6: return L"Scene A Filters";
         case 7: return L"Scene A Envelopes";
         case 8: return L"Scene A LFOs";
         case 9: return L"Scene B Common";
         case 10: return L"Scene B Osc";
         case 11: return L"Scene B Osc Mixer";
         case 12: return L"Scene B Filters";
         case 13: return L"Scene B Envelopes";
         case 14: return L"Scene B LFOs";*/

   createSurge();

   return kResultOk;
}

void SurgeVst3Processor::createSurge()
{
   if (surgeInstance != nullptr)
   {
      return;
   }

   surgeInstance.reset(new SurgeSynthesizer(this));

   if (!surgeInstance.get())
   {
      Surge::UserInteractions::promptError("Unable to allocate SurgeSynthesizer",
                                           "Out of memory");

      return;
   }
}

void SurgeVst3Processor::destroySurge()
{
   surgeInstance.reset();
}

tresult PLUGIN_API SurgeVst3Processor::terminate()
{
   destroySurge();

   return SingleComponentEffect::terminate();
}

tresult PLUGIN_API SurgeVst3Processor::setActive(TBool state)
{
   CHECK_INITIALIZED

   // call our parent setActive
   return SingleComponentEffect::setActive(state);
}

tresult PLUGIN_API SurgeVst3Processor::setProcessing(TBool state)
{
   CHECK_INITIALIZED

   if (!state)
   {
      surgeInstance->allNotesOff();
   }
   else
   {
      blockpos = 0;
   }

   surgeInstance->audio_processing_active = state;

   return SingleComponentEffect::setProcessing(state);
}

tresult PLUGIN_API SurgeVst3Processor::getState(IBStream* state)
{
   CHECK_INITIALIZED

   void* data = nullptr; // surgeInstance keeps its data in an auto-ptr so we don't need to free it
   unsigned int stateSize = surgeInstance->saveRaw(&data);
   state->write(data, stateSize);

   return kResultOk;
}

tresult PLUGIN_API SurgeVst3Processor::setState(IBStream* state)
{
   CHECK_INITIALIZED

   // HACK jsut allocate a meg just to be safe
   const size_t maxsize = 4 * 1024 * 1024;
   void* data = malloc(maxsize);
   int32 numBytes = 0;

   tresult result = state->read(data, maxsize, &numBytes);

   if (result == kResultOk)
   {
      surgeInstance->loadRaw(data, numBytes, false);
   }

   free(data);

   return (result == kResultOk) ? kResultOk : kInternalError;
}

void SurgeVst3Processor::processEvents(int sampleOffset,
                                       Steinberg::Vst::IEventList* events,
                                       int& eventIndex)
{
   if (events)
   {
      int32 numEvents = events->getEventCount();

      while (eventIndex < numEvents)
      {
         Steinberg::Vst::Event e;
         events->getEvent(eventIndex, e);
         if (e.sampleOffset < sampleOffset)
         {
            processEvent(e);
            eventIndex++;
         }
         else
         {
            break;
         }
      }
   }
}

void SurgeVst3Processor::processEvent(const Event& e)
{
   switch (e.type)
   {
   case Event::kNoteOnEvent:
      if (e.noteOn.velocity == 0.f)
      {
         getSurge()->releaseNote(e.noteOn.channel, e.noteOn.pitch, e.noteOn.velocity);
      }
      else
      {
         char cVel = std::min((char)(e.noteOn.velocity * 128.0), (char)127); // Why oh why is this a float in VST3?
         getSurge()->playNote(e.noteOn.channel, e.noteOn.pitch, cVel, e.noteOn.tuning);
      }
      break;

   case Event::kNoteOffEvent:
      getSurge()->releaseNote(e.noteOff.channel, e.noteOff.pitch, e.noteOff.velocity);
      break;

   case Event::kPolyPressureEvent:
      getSurge()->polyAftertouch(e.polyPressure.channel, e.polyPressure.pitch,
                                 e.polyPressure.pressure);
      break;
   }
}

void SurgeVst3Processor::processParameterChanges(int sampleOffset,
                                                 IParameterChanges* parameterChanges,
                                                 int& eventIndex)
{
   if (parameterChanges)
   {
      int32 numParamsChanged = parameterChanges->getParameterCount();
      // for each parameter which are some changes in this audio block:

      for (int32 i = 0; i < numParamsChanged; i++)
      {
         IParamValueQueue* paramQueue = parameterChanges->getParameterData(i);

         if (paramQueue)
         {
            int32 offsetSamples;
            double value = 0;
            ;
            int32 numPoints = paramQueue->getPointCount();

            int id = paramQueue->getParameterId();

            if (id >= MIDI_CONTROLLER_0 && id <= MIDI_CONTROLLER_MAX)
            {
               int chancont = id - MIDI_CONTROLLER_0;
               int channel = chancont & 0xF;
               int cont = chancont >> 4;

               for (int i = 0; i < numPoints; ++i)
               {
                  paramQueue->getPoint(0, offsetSamples, value);
                  if (cont < 128)
                  {
                     if (cont == ControllerNumbers::kCtrlAllSoundsOff ||
                         cont == ControllerNumbers::kCtrlAllNotesOff)
                     {
                        surgeInstance->allNotesOff();
                     }
                     else
                     {
                        surgeInstance->channelController(channel, cont, (int)(value * 128));
                     }
                  }
                  else
                     switch (cont)
                     {
                     case kAfterTouch:
                        surgeInstance->channelAftertouch(channel, (int)(value * 127.f));
                        break;
                     case kPitchBend:
                        surgeInstance->pitchBend(channel, (int)(value * 8192.f));
                        break;
                     case kCtrlProgramChange:
                        break;
                     case kCtrlPolyPressure:
                        break;
                     default:
                        break;
                     }
               }
            }
            else
            {
               int id = paramQueue->getParameterId();

               if (numPoints == 1)
               {
                  paramQueue->getPoint(0, offsetSamples, value);
                  surgeInstance->setParameter01(id, value, true);
               }
               else
               {
                  // Unclear what to do here
               }
            }
         }
      }
   }
}

tresult PLUGIN_API SurgeVst3Processor::process(ProcessData& data)
{
   CHECK_INITIALIZED

   if (data.numInputs == 0 || data.numOutputs == 0)
   {
      // nothing to do
      return kResultOk;
   }

   _fpuState.set();

   int32 numChannels = 2;
   int32 numSamples = data.numSamples;

   surgeInstance->process_input = data.numInputs != 0;

   float** in = surgeInstance->process_input ? data.inputs[0].channelBuffers32 : nullptr;
   float** out = data.outputs[0].channelBuffers32;

   int i;
   int numOutputs = data.numOutputs;
   int numInputs = data.numInputs;
   int noteEventIndex = 0;
   int parameterEventIndex = 0;

   if (data.processContext && data.processContext->state & ProcessContext::kProjectTimeMusicValid)
   {
      surgeInstance->time_data.ppqPos = data.processContext->projectTimeMusic;
   }

   for (i = 0; i < numSamples; i++)
   {
      if (blockpos == 0)
      {
         if (data.processContext)
         {
            double tempo = 120;

            if (data.processContext->state & ProcessContext::kTempoValid)
            {
               tempo = data.processContext->tempo;
            }

            // move clock
            timedata* td = &(surgeInstance->time_data);
            surgeInstance->time_data.tempo = tempo;
            surgeInstance->time_data.ppqPos +=
                (double)BLOCK_SIZE * tempo / (60. * data.processContext->sampleRate);
         }

         processEvents(i, data.inputEvents, noteEventIndex);
         processParameterChanges(i, data.inputParameterChanges, parameterEventIndex);

         surgeInstance->process();
      }

      if (surgeInstance->process_input)
      {
         int inp;
         for (inp = 0; inp < N_INPUTS; inp++)
         {
            surgeInstance->input[inp][blockpos] = in[inp][i];
         }
      }

      int outp;
      for (outp = 0; outp < N_OUTPUTS; outp++)
      {
         out[outp][i] = (float)surgeInstance->output[outp][blockpos];
      }

      blockpos++;
      if (blockpos >= BLOCK_SIZE)
         blockpos = 0;
   }

   // Make sure we iterated over all events
   const int postSampleOffset = 1000000;
   processEvents(postSampleOffset, data.inputEvents, noteEventIndex);
   processParameterChanges(postSampleOffset, data.inputParameterChanges, parameterEventIndex);

   // mark as not silent
   data.outputs[0].silenceFlags = 0;

   _fpuState.restore();

   return kResultOk;
}

tresult PLUGIN_API SurgeVst3Processor::setupProcessing(Steinberg::Vst::ProcessSetup& newSetup)
{
   CHECK_INITIALIZED

   surgeInstance->setSamplerate(newSetup.sampleRate);
   return kResultOk;
}

tresult PLUGIN_API
SurgeVst3Processor::setBusArrangements(Steinberg::Vst::SpeakerArrangement* inputs,
                                       int32 numIns,
                                       Steinberg::Vst::SpeakerArrangement* outputs,
                                       int32 numOuts)
{
   CHECK_INITIALIZED

   if (numIns == 0 && numOuts == 1)
   {
      if (SpeakerArr::getChannelCount(outputs[0]) == 2)
      {
         return kResultOk;
      }
   }
   else if (numIns == 1 && numOuts == 1)
   {
      if ((SpeakerArr::getChannelCount(inputs[0]) == 2) &&
          (SpeakerArr::getChannelCount(outputs[0]) == 2))
      {
         return kResultOk;
      }
   }
   return kResultFalse;
}

IPlugView* PLUGIN_API SurgeVst3Processor::createView(const char* name)
{
   assert(surgeInstance.get());

   if (ConstString(name) == ViewType::kEditor)
   {
      SurgeGUIEditor* editor = new SurgeGUIEditor(this, surgeInstance.get());

      editor->setZoomCallback( [this](SurgeGUIEditor *e) { handleZoom(e); } );
      
      return editor;
   }
   return nullptr;
}

tresult SurgeVst3Processor::beginEdit(ParamID id)
{
   int mappedId =
       SurgeGUIEditor::applyParameterOffset(surgeInstance->remapExternalApiToInternalId(id));
   return Steinberg::Vst::SingleComponentEffect::beginEdit(mappedId);
}

tresult SurgeVst3Processor::performEdit(ParamID id, Steinberg::Vst::ParamValue valueNormalized)
{
   int mappedId =
       SurgeGUIEditor::applyParameterOffset(surgeInstance->remapExternalApiToInternalId(id));
   return Steinberg::Vst::SingleComponentEffect::performEdit(mappedId, valueNormalized);
}

tresult SurgeVst3Processor::endEdit(ParamID id)
{
   int mappedId =
       SurgeGUIEditor::applyParameterOffset(surgeInstance->remapExternalApiToInternalId(id));
   return Steinberg::Vst::SingleComponentEffect::endEdit(mappedId);
}

void SurgeVst3Processor::editorAttached(EditorView* editor)
{
   SurgeGUIEditor* view = dynamic_cast<SurgeGUIEditor*>(editor);
   if (view)
   {
      addDependentView(view);
   }
}

void SurgeVst3Processor::editorRemoved(EditorView* editor)
{
   SurgeGUIEditor* view = dynamic_cast<SurgeGUIEditor*>(editor);
   if (view)
   {
      removeDependentView(view);
   }
}

void SurgeVst3Processor::addDependentView(SurgeGUIEditor* view)
{
   viewsSet.insert(view);
}

void SurgeVst3Processor::removeDependentView(SurgeGUIEditor* view)
{
   viewsSet.erase(view);
}

int32 PLUGIN_API SurgeVst3Processor::getParameterCount()
{
   if (exportAllMidiControllers())
   {
      return getParameterCountWithoutMappings() + kCountCtrlNumber;
   }
   return getParameterCountWithoutMappings();
}

int32 SurgeVst3Processor::getParameterCountWithoutMappings()
{
   return n_total_params + num_metaparameters;
}

tresult PLUGIN_API SurgeVst3Processor::getParameterInfo(int32 paramIndex, ParameterInfo& info)
{
   CHECK_INITIALIZED

   if (paramIndex >= getParameterCount())
   {
      return kInvalidArgument;
   }

   if (isMidiMapController(paramIndex))
   {
      info.flags = 0;
      info.id = 0;
   }

   int id = surgeInstance->remapExternalApiToInternalId(paramIndex);

   parametermeta meta;
   surgeInstance->getParameterMeta(id, meta);

   info.id = id;

   /*
   ** String128 is a TChar[128] is a char16[128]. On mac, wchar is a char32 so
   ** the original reinrpret cast didn't work well.
   **
   ** I thought a lot about using std::wstring_convert<std::codecvt_utf8<wchar_t>> here
   ** but in the end decided to just copy the bytes
   */
   wchar_t tmpwchar[512];
   surgeInstance->getParameterNameW(id, tmpwchar);
   std::copy(tmpwchar, tmpwchar + 128, info.title);

   surgeInstance->getParameterShortNameW(id, tmpwchar);
   std::copy(tmpwchar, tmpwchar + 128, info.shortTitle);

   surgeInstance->getParameterUnitW(id, tmpwchar);
   std::copy(tmpwchar, tmpwchar + 128, info.units);

   info.stepCount = 0; // 1 = toggle,
   info.defaultNormalizedValue = meta.fdefault;
   info.unitId = 0; // meta.clump;
   info.flags = ParameterInfo::kCanAutomate;

   return kResultOk;
}

tresult PLUGIN_API SurgeVst3Processor::getParamStringByValue(ParamID tag,
                                                             ParamValue valueNormalized,
                                                             String128 string)
{
   CHECK_INITIALIZED

   if (tag >= getParameterCount())
   {
      return kInvalidArgument;
   }

   
   wchar_t tmpwchar[ 512 ];
   surgeInstance->getParameterStringW(tag, valueNormalized, tmpwchar);
   std::copy(string, string+128, tmpwchar);

   return kResultOk;
}

tresult PLUGIN_API SurgeVst3Processor::getParamValueByString(ParamID tag,
                                                             TChar* string,
                                                             ParamValue& valueNormalized)
{
   CHECK_INITIALIZED

   if (tag >= getParameterCount())
   {
      return kInvalidArgument;
   }

   return kResultFalse;
}

ParamValue PLUGIN_API SurgeVst3Processor::normalizedParamToPlain(ParamID tag,
                                                                 ParamValue valueNormalized)
{
   ABORT_IF_NOT_INITIALIZED

   if (tag >= getParameterCount())
   {
      return kInvalidArgument;
   }

   return surgeInstance->normalizedToValue(tag, valueNormalized);
}

ParamValue PLUGIN_API SurgeVst3Processor::plainParamToNormalized(ParamID tag, ParamValue plainValue)
{
   ABORT_IF_NOT_INITIALIZED

   if (tag >= getParameterCount())
   {
      return kInvalidArgument;
   }

   return surgeInstance->valueToNormalized(tag, plainValue);
}

ParamValue PLUGIN_API SurgeVst3Processor::getParamNormalized(ParamID tag)
{
   ABORT_IF_NOT_INITIALIZED

   if (tag >= getParameterCount())
   {
      return kInvalidArgument;
   }

   return surgeInstance->getParameter01(surgeInstance->remapExternalApiToInternalId(tag));
}

tresult PLUGIN_API SurgeVst3Processor::setParamNormalized(ParamID tag, ParamValue value)
{
   CHECK_INITIALIZED

   if (tag >= getParameterCount())
   {
      return kInvalidArgument;
   }

   surgeInstance->setParameter01(surgeInstance->remapExternalApiToInternalId(tag), value);

   return kResultOk;
}

SurgeSynthesizer* SurgeVst3Processor::getSurge()
{
   assert(surgeInstance.get() != nullptr);

   return surgeInstance.get();
}

tresult PLUGIN_API SurgeVst3Processor::getMidiControllerAssignment(int32 busIndex,
                                                                   int16 channel,
                                                                   CtrlNumber midiControllerNumber,
                                                                   ParamID& id /*out*/)
{
   /*
   ** Alrighty dighty. What VST3 wants us to do here is, for the controller number,
   ** tell it a parameter to map to. We alas don't have a parameter to map to because
   ** that's not how surge works. But... we can map to parameter id of MIDI_CONTROLLER_0 + id
   ** and test that elsewhere
   */
   id = MIDI_CONTROLLER_0 + midiControllerNumber * 16 + channel;
   return kResultTrue;
}

bool SurgeVst3Processor::exportAllMidiControllers()
{
   return false;
}

bool SurgeVst3Processor::isRegularController(int32 paramIndex)
{
   return paramIndex > 0 && paramIndex < getParameterCountWithoutMappings();
}

bool SurgeVst3Processor::isMidiMapController(int32 paramIndex)
{
   return paramIndex >= getParameterCountWithoutMappings() < getParameterCount();
}

void SurgeVst3Processor::updateDisplay()
{
   // needs IComponentHandler2
   // setDirty(true);
}

void SurgeVst3Processor::setParameterAutomated(int inputParam, float value)
{
   int externalparam = SurgeGUIEditor::unapplyParameterOffset(
       surgeInstance->remapExternalApiToInternalId(inputParam));

   beginEdit(externalparam);
   performEdit(externalparam, value);
   endEdit(externalparam);
}

void SurgeVst3Processor::handleZoom(SurgeGUIEditor *e)
{
    float fzf = e->getZoomFactor() / 100.0;
    int newW = WINDOW_SIZE_X * fzf;
    int newH = WINDOW_SIZE_Y * fzf;


    VSTGUI::CFrame *frame = e->getFrame();
    if(frame)
    {
        frame->setZoom( e->getZoomFactor() / 100.0 );
        frame->setSize(newW, newH);
        /*
        ** rather than calling setSize on myself as in vst2, I have to
        ** inform the plugin frame that I have resized wiht a reference
        ** to a view (which is the editor). This collaborates with
        ** the host to resize once the content is scaled
        */
        Steinberg::IPlugFrame *ipf = e->getIPlugFrame();
        if (ipf)
        {
            Steinberg::ViewRect vr( 0, 0, newW, newH );
            ipf->resizeView( e, &vr );
        }
            
        /*
        ** VSTGUI has an error which is that the background bitmap doesn't get the frame transform
        ** applied. Simply look at cviewcontainer::drawBackgroundRect. So we have to force the background
        ** scale up using a backdoor API.
        */
        
        VSTGUI::CBitmap *bg = frame->getBackground();
        if(bg != NULL)
        {
            CScalableBitmap *sbm = dynamic_cast<CScalableBitmap *>(bg); // dynamic casts are gross but better safe
            if (sbm)
            {
                sbm->setExtraScaleFactor( e->getZoomFactor() );
            }
        }
        
        frame->setDirty( true );
        frame->invalid();
    }
}
