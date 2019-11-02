//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "SurgeStorage.h"
#include "SurgeVoice.h"
#include "effect/Effect.h"
#include "BiquadFilter.h"

struct QuadFilterChainState;

#include <list>
#include <utility>
#include <atomic>

#if TARGET_AUDIOUNIT
class aulayer;
typedef aulayer PluginLayer;
#elif TARGET_VST3
class SurgeVst3Processor;
typedef SurgeVst3Processor PluginLayer;
#elif TARGET_VST2
class Vst2PluginInstance;
using PluginLayer = Vst2PluginInstance;
#elif TARGET_LV2
class SurgeLv2Wrapper;
using PluginLayer = SurgeLv2Wrapper;
#elif TARGET_HEADLESS
class HeadlessPluginLayerProxy;
using PluginLayer = HeadlessPluginLayerProxy;
#else
class PluginLayer;
#endif

struct timedata
{
   double ppqPos, tempo;
};

struct parametermeta
{
   float fmin, fmax, fdefault;
   unsigned int flags, clump;
   bool hide, expert, meta;
};

class alignas(16) SurgeSynthesizer
{
public:
   float output alignas(16)[N_OUTPUTS][BLOCK_SIZE];
   float input alignas(16)[N_INPUTS][BLOCK_SIZE];
   timedata time_data;
   bool audio_processing_active;

   // aligned stuff
   SurgeStorage storage alignas(16);
   lipol_ps FX1 alignas(16),
            FX2 alignas(16),
            amp alignas(16),
            amp_mute alignas(16),
            send alignas(16)[2][2];

   // methods
public:
   SurgeSynthesizer(PluginLayer* parent, std::string suppliedDataPath="");
   virtual ~SurgeSynthesizer();
   void playNote(char channel, char key, char velocity, char detune);
   void releaseNote(char channel, char key, char velocity);
   void releaseNotePostHoldCheck(int scene, char channel, char key, char velocity);
   void pitchBend(char channel, int value);
   void polyAftertouch(char channel, int key, int value);
   void channelAftertouch(char channel, int value);
   void channelController(char channel, int cc, int value);
   void programChange(char channel, int value);
   void allNotesOff();
   void setSamplerate(float sr);
   int getNumInputs()
   {
      return N_INPUTS;
   }
   int getNumOutputs()
   {
      return N_OUTPUTS;
   }
   int getBlockSize()
   {
      return BLOCK_SIZE;
   }
   int getMpeMainChannel(int voiceChannel, int key);
   void process();

   PluginLayer* getParent();

   // protected:

   void onRPN(int channel, int lsbRPN, int msbRPN, int lsbValue, int msbValue);
   void onNRPN(int channel, int lsbNRPN, int msbNRPN, int lsbValue, int msbValue);

   void processControl();
   void processThreadunsafeOperations();
   bool loadFx(bool initp, bool force_reload_all);
   bool loadOscalgos();
   bool load_fx_needed;
   void playVoice(int scene, char channel, char key, char velocity, char detune);
   void releaseScene(int s);
   int calculateChannelMask(int channel, int key);
   void softkillVoice(int scene);
   void enforcePolyphonyLimit(int scene, int margin);
   int getNonUltrareleaseVoices(int scene);
   int getNonReleasedVoices(int scene);

   SurgeVoice* getUnusedVoice(int scene);
   void freeVoice(SurgeVoice*);
   std::array<std::array<SurgeVoice, MAX_VOICES>, 2> voices_array;
   unsigned int voices_usedby[2][MAX_VOICES]; // 0 indicates no user, 1 is scene A & 2 is scene B

   bool
   setParameter01(long index, float value, bool external = false, bool force_integer = false);
   void sendParameterAutomation(long index, float value);
   float getParameter01(long index);
   float getParameter(long index);
   float normalizedToValue(long parameterIndex, float value);
   float valueToNormalized(long parameterIndex, float value);

   void updateDisplay();
   // bool setParameter (long index, float value);
   //	float getParameter (long index);
   bool isValidModulation(long ptag, modsources modsource);
   bool isActiveModulation(long ptag, modsources modsource);
   bool isModsourceUsed(modsources modsource);
   bool isModDestUsed(long moddest);
   ModulationRouting* getModRouting(long ptag, modsources modsource);
   bool setModulation(long ptag, modsources modsource, float value);
   float getModulation(long ptag, modsources modsource);
   float getModDepth(long ptag, modsources modsource);
   void clearModulation(long ptag, modsources modsource);
   void clear_osc_modulation(
       int scene, int entry); // clear the modulation routings on the algorithm-specific sliders
   int remapExternalApiToInternalId(unsigned int x);
   int remapInternalToExternalApiId(unsigned int x);
   void getParameterDisplay(long index, char* text);
   void getParameterDisplay(long index, char* text, float x);
   void getParameterName(long index, char* text);
   void getParameterMeta(long index, parametermeta& pm);
   void getParameterNameW(long index, wchar_t* ptr);
   void getParameterShortNameW(long index, wchar_t* ptr);
   void getParameterUnitW(long index, wchar_t* ptr);
   void getParameterStringW(long index, float value, wchar_t* ptr);
   //	unsigned int getParameterFlags (long index);
   void loadRaw(const void* data, int size, bool preset = false);
   void loadPatch(int id);
   void incrementPatch(bool nextPrev);
   void incrementCategory(bool nextPrev);

   std::string getUserPatchDirectory();
   std::string getLegacyUserPatchDirectory();

   void savePatch();
   void updateUsedState();
   void prepareModsourceDoProcess(int scenemask);
   unsigned int saveRaw(void** data);
   // synth -> editor variables
   std::atomic<int> polydisplay; // updated in audio thread, read from ui, so have assignments be atomic
   bool refresh_editor, patch_loaded;
   int learn_param, learn_custom;
   int refresh_ctrl_queue[8];
   int refresh_parameter_queue[8];
   float refresh_ctrl_queue_value[8];
   bool process_input;
   int patchid_queue;

   float vu_peak[8];

   void populateDawExtraState() {
       storage.getPatch().dawExtraState.isPopulated = true;
       storage.getPatch().dawExtraState.mpeEnabled = mpeEnabled;
       storage.getPatch().dawExtraState.hasTuning = !storage.isStandardTuning;
       if( ! storage.isStandardTuning )
           storage.getPatch().dawExtraState.tuningContents = storage.currentScale.rawText;
       else
           storage.getPatch().dawExtraState.tuningContents = "";
   }
   void loadFromDawExtraState() {
       if( ! storage.getPatch().dawExtraState.isPopulated )
           return;
       mpeEnabled = storage.getPatch().dawExtraState.mpeEnabled;
       if( storage.getPatch().dawExtraState.hasTuning )
       {
           auto sc = Surge::Storage::parseSCLData(storage.getPatch().dawExtraState.tuningContents );
           storage.retuneToScale(sc);
       }
   }
   
public:
   int CC0, PCH, patchid;
   float masterfade = 0;
   HalfRateFilter halfbandA, halfbandB, halfbandIN;
   std::list<SurgeVoice*> voices[2];
   std::unique_ptr<Effect> fx[8];
   bool halt_engine = false;
   MidiChannelState channelState[16];
   bool mpeEnabled = false;
   int mpeVoices = 0;
   int mpePitchBendRange = 0;
   int mpeGlobalPitchBendRange = 0;

   int current_category_id = 0;
   bool modsourceused[n_modsources];
   bool midiprogramshavechanged = false;

   bool switch_toggled_queued, release_if_latched[2], release_anyway[2];
   void setParameterSmoothed(long index, float value);
   BiquadFilter hpA, hpB;

   bool fx_reload[8];   // if true, reload new effect parameters from fxsync
   FxStorage fxsync[8]; // used for synchronisation of parameter init
   int fx_suspend_bitmask;

   // hold pedal stuff

   std::list<std::pair<int,int>> holdbuffer[2];
   void purgeHoldbuffer(int scene);
   quadr_osc sinus;
   int demo_counter = 0;

   QuadFilterChainState* FBQ[2];

   // these have to be thread-safe, so keep private
private:
   PluginLayer* _parent = nullptr;

   void switch_toggled();

   // midicontrol-interpolators
   static const int num_controlinterpolators = 32;
   ControllerModulationSource mControlInterpolator[num_controlinterpolators];
   bool mControlInterpolatorUsed[num_controlinterpolators];

   int GetFreeControlInterpolatorIndex();
   int GetControlInterpolatorIndex(int Idx);
   void ReleaseControlInterpolator(int Idx);
   ControllerModulationSource* ControlInterpolator(int Idx);
   ControllerModulationSource* AddControlInterpolator(int Idx, bool& AlreadyExisted);
};
