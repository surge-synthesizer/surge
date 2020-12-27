#include <gui/SurgeGUIEditor.h>

#include "SurgeVst3Processor.h"
#include "surgecids.h"
#include "UserInteractions.h"

#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/vstpresetkeys.h"

#include "CScalableBitmap.h"

#include <algorithm>
#include <cwchar>
#include <codecvt>
#include <string.h>
#include <cwchar>
#include <stdlib.h>

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

SurgeVst3Processor::SurgeVst3Processor() : surgeInstance()
{
}

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
   // we want a SideChain Input and a Stereo Output
   addAudioInput(STR16("SideChain In"), SpeakerArr::kStereo, kAux );
   addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);
   addAudioOutput(STR16("Scene A Out"), SpeakerArr::kStereo);
   addAudioOutput(STR16("Scene B Out"), SpeakerArr::kStereo);

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

   Steinberg::FUnknownPtr<Steinberg::Vst::IHostApplication> hostApplication(context);
   if (hostApplication)
   {
      std::wstring_convert<std::codecvt_utf8<TChar>,TChar> cv;
      String128 hostName;
      hostApplication->getName(hostName);
      std::string hn8 = cv.to_bytes(hostName);
      surgeInstance->hostProgram = hn8;
   }
   surgeInstance->setupActivateExtraOutputs();
   
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
      Surge::UserInteractions::promptError("Unable to allocate SurgeSynthesizer!",
                                           "Out Of Memory");

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

   /*
   ** I am leaving a substantial amount of debug code in here since
   ** the fix we have placed below is tactical at best and we will
   ** return to this. See #2110
   */
#if 0
   Surge::Debug::openConsole();
   printf( "------ VST3 SET STATE --------\n" ); 
   FUnknownPtr<Steinberg::Vst::IStreamAttributes> sa(state);
   if( ! sa )
      printf( "No StreamAttributes\n" );
   auto ga = [sa](const char* a, char *b) {
                String128 gstring;
                if( sa && sa->getAttributes()->getString( a, gstring, 128*sizeof(TChar)) == kResultTrue )
                {
                   UString128 t(gstring);
                   char ascii[128];
                   t.toAscii(b,128);
                }
                else
                {
                   b[0] = 0;
                }
             };
   char val[256];
   ga( Steinberg::Vst::PresetAttributes::kPlugInName, val ); printf( "PluginName      : '%s'\n", val );
   ga( Steinberg::Vst::PresetAttributes::kName, val );       printf( "Name            : '%s'\n", val );
   ga( Steinberg::Vst::PresetAttributes::kFileName, val );   printf( "FileName        : '%s'\n", val );
   ga( Steinberg::Vst::PresetAttributes::kStateType, val );  printf( "StateType       : '%s'\n", val );
#endif

   
   tresult result = state->read(data, maxsize, &numBytes);

   // printf( "numBytes is %d, result is %d\n", numBytes, result );

   if (result == kResultOk)
   {
#if 0      
      printf( "First 150 bytes of memory chunk\n" );
      for( auto c=0; c<10; ++c )
      {
         char* cd = (char *)data;
         printf( "%4d  : ", c * 15 );
         for( auto cc = 0; cc<15; ++cc )
            printf( "%2x ", (unsigned short)cd[c * 15 + cc ] );
         printf( "  |  " );
         for( auto cc = 0; cc<15; ++cc )
            printf( "%1c", (char)cd[c * 15 + cc ] );
         printf ("\n");
      }
      fflush( stdout );
#endif
      bool isSub3 = ( memcmp(data, "sub3", 4 ) == 0 );

      if( isSub3 )
      {
         // This is the code which used to load on the VST thread. See #3494
         /*
         surgeInstance->loadRaw(data, numBytes, false);
         surgeInstance->loadFromDawExtraState();
         for( auto e : viewsSet )
            e->loadFromDAWExtraState(surgeInstance.get());
            */
         surgeInstance->enqueuePatchForLoad(data, numBytes);
         if( isFirstLoad || ! surgeInstance->audio_processing_active )
         {
            // We are just getting started or don't have audio so force this load to resolve off thread
            surgeInstance->processEnqueuedPatchIfNeeded();
            isFirstLoad = false;
         }
      }
      else
      {
         free(data);
      }
   }
   else
   {
      free(data);
   }

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
         // std::cout << "NoteOff with 0 v " << e.noteOff.velocity << std::endl;
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
   {
      char cPres = value01ToMidi7Bit(e.polyPressure.pressure);
      getSurge()->polyAftertouch(e.polyPressure.channel, e.polyPressure.pitch, cPres);
      break;
   }
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
               

               for (int iPoint = 0; iPoint < numPoints; ++iPoint)
               {
                  paramQueue->getPoint(iPoint, offsetSamples, value);
                  /*
                  if( iPoint == 0 )
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
               int idE = paramQueue->getParameterId();
               // In theory the return false should be all we need here
               SurgeSynthesizer::ID did;
               if( surgeInstance->fromDAWSideId(idE, did ) )
               {
                  paramQueue->getPoint(numPoints - 1, offsetSamples, value);

                  // VST3 wants to send me these events a LOT
                  if( surgeInstance->getParameter01(did) != value )
                     surgeInstance->setParameter01(did, value, true);
               }
               else
               {
                  // std::cout << "SKIPPING SETPARM on " << idE << " - probably a midi cc" << std::endl;
               }
            }
         }
      }
   }
}

tresult PLUGIN_API SurgeVst3Processor::process(ProcessData& data)
{
   CHECK_INITIALIZED

   if (data.numOutputs == 0)
   {
      // nothing to do
      return kResultOk;
   }

   _fpuState.set();

   int32 numSamples = data.numSamples;

   surgeInstance->process_input = data.numInputs != 0 && data.inputs != nullptr;

   float** in = surgeInstance->process_input ? data.inputs[0].channelBuffers32 : nullptr;
   float** out = data.outputs[0].channelBuffers32;

   int i;
   int numOutputs = data.numOutputs;
   int noteEventIndex = 0;
   int parameterEventIndex = 0;

   if (data.processContext &&
       data.processContext->state & ProcessContext::kProjectTimeMusicValid &&
       data.processContext->state & ProcessContext::kPlaying // See #1491 for a discussion of this choice
      )
   {
      surgeInstance->time_data.ppqPos = data.processContext->projectTimeMusic;
   }
   if (data.processContext && data.processContext->state & ProcessContext::kTimeSigValid)
   {
      surgeInstance->time_data.timeSigNumerator = data.processContext->timeSigNumerator;
      surgeInstance->time_data.timeSigDenominator = data.processContext->timeSigDenominator;
   }
   else
   {
      surgeInstance->time_data.timeSigNumerator = 4;
      surgeInstance->time_data.timeSigDenominator = 4;
   }

   double tempo = 120;
   if (data.processContext && data.processContext->state & ProcessContext::kTempoValid)
   {
      tempo = data.processContext->tempo;
   }

   // move clock
   surgeInstance->time_data.tempo = tempo;
   surgeInstance->resetStateFromTimeData();

   for (i = 0; i < numSamples; i++)
   {
      if (blockpos == 0)
      {
         if (data.processContext)
         {
            surgeInstance->time_data.ppqPos +=
                (double)BLOCK_SIZE * tempo / (60. * data.processContext->sampleRate);
         }

         processEvents(i, data.inputEvents, noteEventIndex);
         processParameterChanges(i, data.inputParameterChanges, parameterEventIndex);

         surgeInstance->process();
      }

      if (surgeInstance->process_input && in)
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
      // TODO: FIX SCENE ASSUMPTION, if we have more scenes, each should get its own additional output eventually!
      if( numOutputs == 3 )
      {
         float** outA = data.outputs[1].channelBuffers32;
         float** outB = data.outputs[2].channelBuffers32;
         if (surgeInstance->activateExtraOutputs)
         {
            for (int ch = 0; ch < 2; ++ch)
            {
               outA[ch][i] = (float)surgeInstance->sceneout[0][ch][blockpos];
               outB[ch][i] = (float)surgeInstance->sceneout[1][ch][blockpos];
            }
         }
         else
         {
            for (int ch = 0; ch < 2; ++ch)
            {
               outA[ch][i] = 0.f;
               outB[ch][i] = 0.f;
            }
         }
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
   // TODO: FIX SCENE ASSUMPTION
   if( numOutputs == 3 )
   {
      data.outputs[1].silenceFlags = 0;
      data.outputs[2].silenceFlags = 0;
   }

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
      editor->setZoomCallback( [this](SurgeGUIEditor *e, bool resizeWindow) { redraw(e, resizeWindow); } );

      if(lastZoom > 0)
          editor->setZoomFactor(lastZoom);

      return editor;
   }
   return nullptr;
}

// This is now called with the dawID
tresult SurgeVst3Processor::beginEdit(ParamID synid)
{
   SurgeSynthesizer::ID did;

   if( !SurgeGUIEditor::fromSynthGUITag(surgeInstance.get(), synid, did ))
      return kResultFalse;
   char txt[256];
   surgeInstance->getParameterName(did, txt);

   int id = did.getDawSideId();

   if( beginEditGuard.find(id) == beginEditGuard.end() )
   {
       beginEditGuard[id] = 0;
   }
   beginEditGuard[id] ++;

   if( beginEditGuard[id] == 1 )
   {
       return Steinberg::Vst::SingleComponentEffect::beginEdit(id);
   }
   else
   {
       return kResultOk;
   }
}

tresult SurgeVst3Processor::performEdit(ParamID synid, Steinberg::Vst::ParamValue valueNormalized)
{
   SurgeSynthesizer::ID did;

   if( !SurgeGUIEditor::fromSynthGUITag(surgeInstance.get(), synid, did ))
      return kResultFalse;

   int id = did.getDawSideId();

   char txt[256];
   surgeInstance->getParameterName(did, txt);

   return Steinberg::Vst::SingleComponentEffect::performEdit(id, valueNormalized);
}

tresult SurgeVst3Processor::endEdit(ParamID synid)
{
   SurgeSynthesizer::ID did;

   if( !SurgeGUIEditor::fromSynthGUITag(surgeInstance.get(), synid, did ))
      return kResultFalse;

   int id = did.getDawSideId();

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

   if( endcount == 0 )
   {
      // std::cout << "EndEdit " << mappedId << std::endl;
      return Steinberg::Vst::SingleComponentEffect::endEdit(id);
   }
   else
   {
      return kResultOk;
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
   // Concerning - reaper calls this all the time!
   // std::cout << __LINE__ << " getParameterInfo " << paramIndex << std::endl;
   // stackToInfo();
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
      char nm8[512];
      wchar_t nm[512];
      auto im = paramIndex - midi_controller_0;
      auto ich = im % 16;
      auto icc = im / 16;
      
      snprintf(nm8, 512, "MIDI CC %d, Channel %d", icc, ich + 1);
      swprintf(nm, 512, L"%s", nm8);
#if MAC || LINUX
      std::copy(nm, nm + 128, info.title);
#else
      swprintf(reinterpret_cast<wchar_t *>(info.title), 128, L"%S", nm);
#endif
      
   }
   else
   {
      SurgeSynthesizer::ID did;
      if( ! surgeInstance->fromDAWSideIndex(paramIndex, did ) )
      {
         // std::cout << "UNABLE TO GET by Index" << std::endl;
         return kInvalidArgument;
      }

      parametermeta meta;
      surgeInstance->getParameterMeta(did, meta);
      
      info.id = did.getDawSideId();
      
      /*
      ** String128 is a TChar[128] is a char16[128]. On mac, wchar is a char32 so
      ** the original reinrpret cast didn't work well.
      **
      ** I thought a lot about using std::wstring_convert<std::codecvt_utf8<wchar_t>> here
      ** but in the end decided to just copy the bytes
      */
      wchar_t tmpwchar[512];
      surgeInstance->getParameterNameW(did, tmpwchar);
#if MAC || LINUX
      std::copy(tmpwchar, tmpwchar + 128, info.title);
#else
      swprintf(reinterpret_cast<wchar_t *>(info.title), 128, L"%S", tmpwchar);
#endif   
      
      surgeInstance->getParameterShortNameW(did, tmpwchar);
#if MAC || LINUX
      std::copy(tmpwchar, tmpwchar + 128, info.shortTitle);
#else
      swprintf(reinterpret_cast<wchar_t *>(info.shortTitle), 128, L"%S", tmpwchar);
#endif   
      
      surgeInstance->getParameterUnitW(did, tmpwchar);
#if MAC || LINUX
      std::copy(tmpwchar, tmpwchar + 128, info.units);
#else
      swprintf(reinterpret_cast<wchar_t *>(info.units), 128, L"%S", tmpwchar);
#endif   
      
      info.stepCount = 0; // 1 = toggle,
      info.defaultNormalizedValue = meta.fdefault;
      info.unitId = 0; // meta.clump;
      info.flags = ParameterInfo::kCanAutomate;

      //char nm[512];
      //surgeInstance->getParameterName( id, nm );
      // std::cout << "gPID " << paramIndex << " "  << " " << info.id << std::endl;
            

   }
   return kResultOk;
}

tresult PLUGIN_API SurgeVst3Processor::getParamStringByValue(ParamID tag,
                                                             ParamValue valueNormalized,
                                                             String128 ontostring)
{
   // std::cout << __LINE__ << " " << __func__ << " " << tag << std::endl;
   CHECK_INITIALIZED;

   if( tag >= metaparam_offset && tag <= metaparam_offset + num_metaparameters ) 
   {
   }
   else if (tag >= getParameterCount() )
   {
      return kInvalidArgument;
   }
   else if( tag >= getParameterCountWithoutMappings() )
   {
      return kResultOk;
   }
   
   wchar_t tmpwchar[ 512 ];
   SurgeSynthesizer::ID did;
   if( surgeInstance->fromDAWSideId(tag, did ))
   {
      surgeInstance->getParameterStringW(did, valueNormalized, tmpwchar);
#if MAC || LINUX
      std::copy(tmpwchar, tmpwchar + 128, ontostring);
#else
      swprintf(reinterpret_cast<wchar_t*>(ontostring), 128, L"%S", tmpwchar);
#endif
   }
   return kResultOk;
}

tresult PLUGIN_API SurgeVst3Processor::getParamValueByString(ParamID tag,
                                                             TChar* string,
                                                             ParamValue& valueNormalized)
{
   // std::cout << __LINE__ << " " << __func__ << " " << tag << std::endl;
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
   // std::cout << __LINE__ << " " << __func__ << " " << tag << std::endl;
   ABORT_IF_NOT_INITIALIZED;

   ParamValue v = 0;
   SurgeSynthesizer::ID did;
   if( surgeInstance->fromDAWSideId(tag, did ))
      v = surgeInstance->normalizedToValue(did, valueNormalized);
   return v;
}

ParamValue PLUGIN_API SurgeVst3Processor::plainParamToNormalized(ParamID tag, ParamValue plainValue)
{
   // std::cout << __LINE__ << " " << __func__ << " " << tag << " " << plainValue << std::endl;
   ABORT_IF_NOT_INITIALIZED

   ParamValue v = 0;
   SurgeSynthesizer::ID did;
   if( surgeInstance->fromDAWSideId(tag, did ))
      v = surgeInstance->valueToNormalized(did, plainValue);
   // std::cout << __LINE__ << " " << __func__ << " " << tag << " " << plainValue << " = " << res << std::endl;
   return v;
}

ParamValue PLUGIN_API SurgeVst3Processor::getParamNormalized(ParamID tag)
{
   // std::cout << __LINE__ << " getParamNormalized " << tag << std::endl;
   ABORT_IF_NOT_INITIALIZED;

   ParamValue res = 0;
   SurgeSynthesizer::ID did;
   if( surgeInstance->fromDAWSideId(tag, did ))
      res = surgeInstance->getParameter01(did);
   //std::cout << __LINE__ << " getParamNormalized " << tag << " = " << res << " " << floatBytes(res) << std::endl;
   //stackToInfo();
   return res;
}

tresult PLUGIN_API SurgeVst3Processor::setParamNormalized(ParamID tag, ParamValue value)
{
   // std::cout << __LINE__ << " " << __func__ << " " << tag << " " << value << std::endl;
   CHECK_INITIALIZED;

   ParamValue v = 0;
   SurgeSynthesizer::ID did;
   if( surgeInstance->fromDAWSideId(tag, did ))
   {
      surgeInstance->setParameter01(did, value, true);
      return kResultOk;
   }

   return kResultFalse;
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
   // Why even ask me? But you do. So...
   if( midiControllerNumber >= kCountCtrlNumber || midiControllerNumber < 0 )
      return kResultFalse;
   
   /*
   ** Alrighty dighty. What VST3 wants us to do here is, for the controller number,
   ** tell it a parameter to map to. We alas don't have a parameter to map to because
   ** that's not how surge works. But... we can map to parameter id of midi_controller_0 + id
   ** and test that elsewhere
   */

   id = midi_controller_0 + midiControllerNumber * 16 + channel;
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
   /*
   ** This particular choice of implementation is why we have nested
   ** begin/end pairs with the guard. We discovered this clsoing in on
   ** 1.6.2 and decided to leave it and add the guard, but a future version
   ** of ourselves should think deeply about how we want to implement this,
   ** since neither AU or VST2 use this approach
    *
    * More over with DAW interface, we get called here with a different ID
    * structure (the dawID in this case) than the raw GUI beginedit so expand
    * the implementation
   */
   if( beginEditGuard.find(inputParam) == beginEditGuard.end() )
   {
      Steinberg::Vst::SingleComponentEffect::beginEdit(inputParam);
      Steinberg::Vst::SingleComponentEffect::performEdit(inputParam, value);
      Steinberg::Vst::SingleComponentEffect::endEdit(inputParam);
   }
   else
   {
      Steinberg::Vst::SingleComponentEffect::performEdit(inputParam, value);
   }
}

void SurgeVst3Processor::resize(SurgeGUIEditor *e, int width, int heigth)
{
	/*
	** rather than calling setSize on myself as in vst2, I have to
	** inform the plugin frame that I have resized with a reference
	** to a view (which is the editor). This collaborates with
	** the host to resize once the content is scaled
	*/
	Steinberg::IPlugFrame *ipf = e->getIPlugFrame();
	if (ipf)
	{
		Steinberg::ViewRect vr(0, 0, width, heigth);
		Steinberg::tresult res = ipf->resizeView(e, &vr);
		if (res != Steinberg::kResultTrue)
		{
			// Leaving this here for debug purposes. resizeView() can fail in non-fatal way and zoom reset is just too harsh.
			/*std::ostringstream oss;
			oss << "Your host failed to zoom VST3 to " << e->getZoomFactor() << " scale. "
				<< "Surge will now attempt to reset the zoom level to 100%. You may see several "
				<< "other error messages in the course of this being resolved.";
			Surge::UserInteractions::promptError(oss.str(), "VST3 Host Zoom Error" );
			e->setZoomFactor(100);*/
		}
	}
}

void SurgeVst3Processor::redraw(SurgeGUIEditor *e, bool resizeWindow)
{
    VSTGUI::CFrame *frame = e->getFrame();
    if(frame)
    {
		float fzf = e->getZoomFactor() / 100.0;
		int width = e->getWindowSizeX() * fzf;
		int height = e->getWindowSizeY() * fzf;

        frame->setZoom(fzf);
        frame->setSize(width, height);

		if (resizeWindow)
		{
                   if( ! e->hasIdleRun )
                   {
                      e->resizeToOnIdle = VSTGUI::CPoint( width, height );
                   }
                   else
                   {
                      resize(e, width, height);
                   }
		}

		setExtraScaleFactor(frame->getBackground(), e->getZoomFactor());

        frame->setDirty( true );
        frame->invalid();
        lastZoom = e->getZoomFactor();
    }
}

void SurgeVst3Processor::setExtraScaleFactor(VSTGUI::CBitmap *bg, float zf)
{
	if (bg != NULL)
	{
		auto sbm = dynamic_cast<CScalableBitmap *>(bg); // dynamic casts are gross but better safe
		if (sbm)
		{
			/*
			** VSTGUI has an error which is that the background bitmap doesn't get the frame transform
			** applied. Simply look at cviewcontainer::drawBackgroundRect. So we have to force the background
			** scale up using a backdoor API.
			*/
			sbm->setExtraScaleFactor(zf);
		}
	}
}

void SurgeVst3Processor::uithreadIdleActivity()
{
   if (checkNamesEvery++ == 2)
   {
      checkNamesEvery = 0;
      if (std::atomic_exchange(&parameterNameUpdated, false))
      {
         auto comph = getComponentHandler();
         if (comph)
         {
            comph->restartComponent(kParamTitlesChanged | kParamValuesChanged);
         }
      }
   }
}