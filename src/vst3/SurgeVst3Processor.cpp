#include <gui/SurgeGUIEditor.h>

#include "SurgeVst3Processor.h"
#include "surgecids.h"

#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/base/ustring.h"

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
   addEventInput(STR16("Note In"));

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
      // out of memory
#if WIN32
      MessageBoxW(::GetActiveWindow(), L"Could not allocate memory.", L"Out of memory",
                  MB_OK | MB_ICONERROR);
#endif

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
         getSurge()->playNote(e.noteOn.channel, e.noteOn.pitch, e.noteOn.velocity, e.noteOn.tuning);
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
            double value;
            int32 numPoints = paramQueue->getPointCount();
            /*switch (paramQueue->getParameterId ())
            {

            }*/
         }
      }
   }
   /*int32 numParamsChanged = paramChanges->getParameterCount ();
     // for each parameter which are some changes in this audio block:
     for (int32 i = 0; i < numParamsChanged; i++)
     {
        IParamValueQueue* paramQueue = paramChanges->getParameterData (i);
        if (paramQueue)
        {
           int32 offsetSamples;
           double value;
           int32 numPoints = paramQueue->getPointCount ();
           switch (paramQueue->getParameterId ())
           {
           case kGainId:
              // we use in this example only the last point of the queue.
              // in some wanted case for specific kind of parameter it makes sense to retrieve all
     points
              // and process the whole audio block in small blocks.
              if (paramQueue->getPoint (numPoints - 1,  offsetSamples, value) == kResultTrue)
              {
                 fGain = (float)value;
              }
              break;

           case kBypassId:
              if (paramQueue->getPoint (numPoints - 1,  offsetSamples, value) == kResultTrue)
              {
                 bBypass = (value > 0.5f);
              }
              break;
           }
        }
     }*/
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
                (double)block_size * tempo / (60. * data.processContext->sampleRate);
         }

         processEvents(i, data.inputEvents, noteEventIndex);
         processParameterChanges(i, data.inputParameterChanges, parameterEventIndex);

         surgeInstance->process();
      }

      if (surgeInstance->process_input)
      {
         int inp;
         for (inp = 0; inp < n_inputs; inp++)
         {
            surgeInstance->input[inp][blockpos] = in[inp][i];
         }
      }

      int outp;
      for (outp = 0; outp < n_outputs; outp++)
      {
         out[outp][i] = (float)surgeInstance->output[outp][blockpos];
      }

      blockpos++;
      if (blockpos >= block_size)
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

      return editor;
   }
   return nullptr;
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
{}

void SurgeVst3Processor::removeDependentView(SurgeGUIEditor* view)
{}

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

   surgeInstance->getParameterNameW(id, reinterpret_cast<wchar_t *>(info.title));
   surgeInstance->getParameterShortNameW(id, reinterpret_cast<wchar_t *>(info.shortTitle));
   surgeInstance->getParameterUnitW(id, reinterpret_cast<wchar_t *>(info.units));
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

   surgeInstance->getParameterStringW(tag, valueNormalized, reinterpret_cast<wchar_t *>(string));

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
   return kResultFalse;
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

void SurgeVst3Processor::setParameterAutomated(int externalparam, float value)
{
   beginEdit(externalparam); // TODO
   performEdit(externalparam, value);
   endEdit(externalparam);
}
