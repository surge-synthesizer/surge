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

   disableZoom = false;
#if WINDOWS
   Steinberg::FUnknownPtr<Steinberg::Vst::IHostApplication> hostApplication(context);
   if (hostApplication)
   {
      String128 hostName;
      hostApplication->getName(hostName);
      char szString[256];
      size_t nNumCharConverted;
      wcstombs_s(&nNumCharConverted, szString, 256, hostName, 128);
      std::string s(szString);
      disableZoom = false;
      //if (s == "Cakewalk")
      //   disableZoom = true;
   }
#endif

   //---create Audio In/Out busses------
   // we want a SideChain Input and a Stereo Output
   addAudioInput(STR16("SideChain In"), SpeakerArr::kStereo, kAux );
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

   midi_controller_0 = getParameterCountWithoutMappings();
   midi_controller_max = midi_controller_0 + n_midi_controller_params;
   
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
   surgeInstance->populateDawExtraState();
   for( auto e : viewsSet )
       e->populateDawExtraState(surgeInstance.get());

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
      surgeInstance->loadFromDawExtraState();
      for( auto e : viewsSet )
          e->loadFromDAWExtraState(surgeInstance.get());

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

static int value01ToMidi7Bit(double x)
{
   return std::min(127, std::max(0, (int)(x * 127.0)));
}

static int value01ToMidi14Bit(double x)
{
   return std::min(16383, std::max(0, (int)(x * 16383.0)));
}

void SurgeVst3Processor::processEvent(const Event& e)
{
   switch (e.type)
   {
   case Event::kNoteOnEvent:
      if (e.noteOn.velocity == 0.f)
      {
         std::cout << "NoteOff with 0 v " << e.noteOff.velocity << std::endl;
         getSurge()->releaseNote(e.noteOn.channel, e.noteOn.pitch, e.noteOn.velocity);
      }
      else
      {
         // Why oh why is this a float in VST3?
         char cVel = value01ToMidi7Bit(e.noteOn.velocity);
         getSurge()->playNote(e.noteOn.channel, e.noteOn.pitch, cVel, e.noteOn.tuning);
      }
      break;

   case Event::kNoteOffEvent:
   {
      char cVel = value01ToMidi7Bit(e.noteOff.velocity);
      getSurge()->releaseNote(e.noteOff.channel, e.noteOff.pitch, cVel);
      break;
   }

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
            int32 numPoints = paramQueue->getPointCount();

            int id = paramQueue->getParameterId();

            
            if (id >= midi_controller_0 && id <= midi_controller_max)
            {
               int chancont = id - midi_controller_0;
               int channel = chancont & 0xF;
               int cont = chancont >> 4;
               

               for (int i = 0; i < numPoints; ++i)
               {
                  paramQueue->getPoint(0, offsetSamples, value);
                  /*
                  if( i == 0 ) 
                  {
                     std::cout << "MIDI id=" << id << " chancont=" << chancont << " channel=" << channel << " controller=" << cont << " value=" << value << std::endl;
                  }
                  */
                  
                  if (cont < 128)
                  {
                     if (cont == ControllerNumbers::kCtrlAllSoundsOff ||
                         cont == ControllerNumbers::kCtrlAllNotesOff)
                     {
                        surgeInstance->allNotesOff();
                     }
                     else
                     {
                        surgeInstance->channelController(channel, cont, value01ToMidi7Bit(value));
                     }
                  }
                  else
                     switch (cont)
                     {
                     case kAfterTouch:
                        surgeInstance->channelAftertouch(channel, value01ToMidi7Bit(value));
                        break;
                     case kPitchBend:
                        /*
                        ** VST3 float value is between 0 and 1, pitch bend is between -1 and 1. Center it
                        */
                        surgeInstance->pitchBend(channel, value01ToMidi14Bit(value) - 8192);
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
               if ( id < getParameterCountWithoutMappings() )
               {
                  // Make a choice to use the latest point not the earliest. I think that's right. (I actually
                  // don't think it matters all that much but hey...)
                  paramQueue->getPoint(numPoints - 1, offsetSamples, value);

                  // VST3 wants to send me these events a LOT
                  if( surgeInstance->getParameter01(id) != value )
                     surgeInstance->setParameter01(id, value, true);
               }
               else
               {
                  // std::cerr << "Unable to handle parameter " << id << " with npoints " << numPoints << std::endl;
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

            if (data.processContext->state & ProcessContext::kTimeSigValid)
            {
               surgeInstance->time_data.timeSigNumerator = data.processContext->timeSigNumerator;
               surgeInstance->time_data.timeSigDenominator = data.processContext->timeSigDenominator;
            }
            else
            {
               surgeInstance->time_data.timeSigNumerator = 4;
               surgeInstance->time_data.timeSigDenominator = 4;
            }
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

      if (disableZoom)
         editor->disableZoom();

      editor->setZoomCallback( [this](SurgeGUIEditor *e) { handleZoom(e); } );

      if( haveZoomed )
          editor->setZoomFactor(lastZoom);
      
      return editor;
   }
   return nullptr;
}

tresult SurgeVst3Processor::beginEdit(ParamID id)
{
   if( beginEditGuard.find(id) == beginEditGuard.end() )
   {
       beginEditGuard[id] = 0;
   }
   beginEditGuard[id] ++;
   if (id >= getParameterCount())
   {
      return kInvalidArgument;
   }
   if( id >= getParameterCountWithoutMappings() )
   {
      return kResultOk;
   }

   int mappedId =
       SurgeGUIEditor::applyParameterOffset(surgeInstance->remapExternalApiToInternalId(id));
   if( beginEditGuard[id] == 1 )
   {
       return Steinberg::Vst::SingleComponentEffect::beginEdit(mappedId);
   }
   else
   {
       return kResultOk;
   }
}

tresult SurgeVst3Processor::performEdit(ParamID id, Steinberg::Vst::ParamValue valueNormalized)
{
   if (id >= getParameterCount())
   {
      return kInvalidArgument;
   }
   if( id >= getParameterCountWithoutMappings() )
   {
      return kResultOk;
   }

   int mappedId =
       SurgeGUIEditor::applyParameterOffset(surgeInstance->remapExternalApiToInternalId(id));
   return Steinberg::Vst::SingleComponentEffect::performEdit(mappedId, valueNormalized);
}

tresult SurgeVst3Processor::endEdit(ParamID id)
{
   if (id >= getParameterCount() )
   {
      return kInvalidArgument;
   }

   auto endcount = -1;
   if( beginEditGuard.find(id) == beginEditGuard.end() )
   {
       // this is a pretty bad software error
       std::cerr << "End called with no matchign begin" << std::endl;
       return kResultFalse;
   }
   else
   {
       beginEditGuard[id] --;
       endcount = beginEditGuard[id];
   };

   if( id > getParameterCountWithoutMappings() )
   {
      return kResultOk;
   }
   else
   {
      int mappedId =
         SurgeGUIEditor::applyParameterOffset(surgeInstance->remapExternalApiToInternalId(id));
      if( endcount == 0 )
      {
         return Steinberg::Vst::SingleComponentEffect::endEdit(mappedId);
      }
      else
      {
         return kResultOk;
      }
   }
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
   return getParameterCountWithoutMappings() + n_midi_controller_params;
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
      info.id = paramIndex;

      info.stepCount = 0; // 1 = toggle,
      info.unitId = 0; // meta.clump;
      
      // FIXME - set the title
   }
   else
   {
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
#if MAC || LINUX
      std::copy(tmpwchar, tmpwchar + 128, info.title);
#else
      swprintf(reinterpret_cast<wchar_t *>(info.title), 128, L"%S", tmpwchar);
#endif   
      
      surgeInstance->getParameterShortNameW(id, tmpwchar);
#if MAC || LINUX
      std::copy(tmpwchar, tmpwchar + 128, info.shortTitle);
#else
      swprintf(reinterpret_cast<wchar_t *>(info.shortTitle), 128, L"%S", tmpwchar);
#endif   
      
      surgeInstance->getParameterUnitW(id, tmpwchar);
#if MAC || LINUX
      std::copy(tmpwchar, tmpwchar + 128, info.units);
#else
      swprintf(reinterpret_cast<wchar_t *>(info.units), 128, L"%S", tmpwchar);
#endif   
      
      info.stepCount = 0; // 1 = toggle,
      info.defaultNormalizedValue = meta.fdefault;
      info.unitId = 0; // meta.clump;
      info.flags = ParameterInfo::kCanAutomate;
   }
   return kResultOk;
}

tresult PLUGIN_API SurgeVst3Processor::getParamStringByValue(ParamID tag,
                                                             ParamValue valueNormalized,
                                                             String128 ontostring)
{
   CHECK_INITIALIZED;

   if (tag >= getParameterCount() )
   {
      return kInvalidArgument;
   }

   if( tag >= getParameterCountWithoutMappings() )
   {
      return kResultOk;
   }
   
   wchar_t tmpwchar[ 512 ];
   surgeInstance->getParameterStringW(tag, valueNormalized, tmpwchar);
#if MAC || LINUX
   std::copy(tmpwchar, tmpwchar+128, ontostring );
#else
   swprintf(reinterpret_cast<wchar_t *>(ontostring), 128, L"%S", tmpwchar);
#endif   

   return kResultOk;
}

tresult PLUGIN_API SurgeVst3Processor::getParamValueByString(ParamID tag,
                                                             TChar* string,
                                                             ParamValue& valueNormalized)
{
   CHECK_INITIALIZED;
      
   if (tag >= getParameterCount())
   {
      return kInvalidArgument;
   }

   return kResultFalse;
}

ParamValue PLUGIN_API SurgeVst3Processor::normalizedParamToPlain(ParamID tag,
                                                                 ParamValue valueNormalized)
{
   ABORT_IF_NOT_INITIALIZED;

   if (tag >= getParameterCount())
   {
      return kInvalidArgument;
   }
   if( tag >= getParameterCountWithoutMappings())
   {
      return 0;
   }

   return surgeInstance->normalizedToValue(tag, valueNormalized);
}

ParamValue PLUGIN_API SurgeVst3Processor::plainParamToNormalized(ParamID tag, ParamValue plainValue)
{
   ABORT_IF_NOT_INITIALIZED

   if (tag >= getParameterCountWithoutMappings())
   {
       // return kInvalidArgument;
       // kInvalidArgument is not a ParamValue. In this case just
       return 0;
   }

   return surgeInstance->valueToNormalized(tag, plainValue);
}

ParamValue PLUGIN_API SurgeVst3Processor::getParamNormalized(ParamID tag)
{
   ABORT_IF_NOT_INITIALIZED;

   if (tag >= getParameterCountWithoutMappings())
   {
      // return kInvalidArgument;
      // kInvalidArgument is not a ParamValue. In this case just
      return 0;
   }

   auto res = surgeInstance->getParameter01(tag);
   return res;
}

tresult PLUGIN_API SurgeVst3Processor::setParamNormalized(ParamID tag, ParamValue value)
{
   CHECK_INITIALIZED;

   if (tag >= getParameterCount())
   {
      return kInvalidArgument;
   }
   if( tag >= getParameterCountWithoutMappings() )
   {
      return kResultOk;
   }

   /*
   ** Priod code had this:
   **
   ** surgeInstance->setParameter01(surgeInstance->remapExternalApiToInternalId(tag), value);
   **
   ** which remaps "control 0" -> 2048. I think that's right for the VST2 but for the VST3 where
   ** we are specially dealing with midi controls it is the wrong thing to do; it makes the FX
   ** control and the control 0 the same. So here just pass the tag on directly.
   */
   if( value != surgeInstance->getParameter01(tag) )
      surgeInstance->setParameter01(tag, value, true);

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
   ** that's not how surge works. But... we can map to parameter id of midi_controller_0 + id
   ** and test that elsewhere
   */

   id = midi_controller_0 + midiControllerNumber * 16 + channel;
   // std::cout << "getMidiControllerAssignment " << channel << " midiControllerNumber=" << midiControllerNumber << " id=" << id << std::endl;
   return kResultTrue;
}

bool SurgeVst3Processor::isRegularController(int32 paramIndex)
{
   return paramIndex > 0 && paramIndex < getParameterCountWithoutMappings();
}

bool SurgeVst3Processor::isMidiMapController(int32 paramIndex)
{
   return paramIndex >= getParameterCountWithoutMappings() && paramIndex < getParameterCount();
}

void SurgeVst3Processor::updateDisplay()
{
   // needs IComponentHandler2
   // setDirty(true);
}

void SurgeVst3Processor::setParameterAutomated(int inputParam, float value)
{
   if( inputParam >= getParameterCountWithoutMappings() ) return;
   
   int externalparam = SurgeGUIEditor::unapplyParameterOffset(
       surgeInstance->remapExternalApiToInternalId(inputParam));

   /*
   ** This particular choice of implementation is why we have nested
   ** begin/end pairs with the guard. We discovered this clsoing in on
   ** 1.6.2 and decided to leave it and add the guard, but a future version
   ** of ourselves should think deeply about how we want to implement this,
   ** since neither AU or VST2 use this approach
   */
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
            Steinberg::tresult res = ipf->resizeView(e, &vr);
            if (res != Steinberg::kResultTrue)
               Surge::UserInteractions::promptError("Your host failed to zoom VST3", "Host Error");
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

        haveZoomed = true;
        lastZoom = e->getZoomFactor();
    }
}
