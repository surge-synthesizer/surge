//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "AbstractSynthesizer.h"
#include "SurgeStorage.h"
#include "SurgeVoice.h"
#include "effect/Effect.h"
#include "BiquadFilter.h"

struct QuadFilterChainState;

#include <list>
using namespace std;

const int max_voices = 64;

#if TARGET_AUDIOUNIT
class aulayer;
typedef aulayer PluginLayer;
#elif TARGET_VST3
class SurgeVst3Processor;
typedef SurgeVst3Processor PluginLayer;
#elif TARGET_VST2
class Vst2PluginInstance;
using PluginLayer = Vst2PluginInstance;
#else
class PluginLayer;
#endif

class SurgeSynthesizer : public AbstractSynthesizer
{
public:
   // aligned stuff
   SurgeStorage storage alignas(16);
   lipol_ps FX1 alignas(16),
            FX2 alignas(16),
            amp alignas(16),
            amp_mute alignas(16),
            send alignas(16)[2][2];

   // methods
public:
   SurgeSynthesizer(PluginLayer* parent);
   virtual ~SurgeSynthesizer();
   void playNote(char channel, char key, char velocity, char detune) override;
   void releaseNote(char channel, char key, char velocity) override;
   void releaseNotePostHoldCheck(int scene, char channel, char key, char velocity);
   void pitchBend(char channel, int value) override;
   void polyAftertouch(char channel, int key, int value) override;
   void channelAftertouch(char channel, int value) override;
   void channelController(char channel, int cc, int value) override;
   void programChange(char channel, int value) override;
   void allNotesOff() override;
   void setSamplerate(float sr) override;
   int getNumInputs() override
   {
      return n_inputs;
   }
   int getNumOutputs() override
   {
      return n_outputs;
   }
   int getBlockSize() override
   {
      return block_size;
   }
   int getMpeMainChannel(int voiceChannel, int key);
   void process() override;

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
   SurgeVoice* voices_array[2][max_voices];
   unsigned int voices_usedby[2][max_voices]; // 0 indicates no user, 1 is scene A & 2 is scene B

   bool
   setParameter01(long index, float value, bool external = false, bool force_integer = false) final;
   void sendParameterAutomation(long index, float value);
   float getParameter01(long index) final;
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
   int remapExternalApiToInternalId(unsigned int x) override;
   int remapInternalToExternalApiId(unsigned int x);
   void getParameterDisplay(long index, char* text) override;
   void getParameterDisplay(long index, char* text, float x) override;
   void getParameterName(long index, char* text) override;
   void getParameterMeta(long index, parametermeta& pm) override;
   void getParameterNameW(long index, wchar_t* ptr);
   void getParameterShortNameW(long index, wchar_t* ptr);
   void getParameterUnitW(long index, wchar_t* ptr);
   void getParameterStringW(long index, float value, wchar_t* ptr);
   //	unsigned int getParameterFlags (long index);
   void loadRaw(const void* data, int size, bool preset = false) override;
   void loadPatch(int id);
   void incrementPatch(int category, int patch);

   string getUserPatchDirectory();
   string getLegacyUserPatchDirectory();

   void savePatch();
   void updateUsedState();
   void prepareModsourceDoProcess(int scenemask);
   unsigned int saveRaw(void** data) override;
   // synth -> editor variables
   int polydisplay;
   bool refresh_editor, patch_loaded;
   int learn_param, learn_custom;
   int refresh_ctrl_queue[8];
   int refresh_parameter_queue[8];
   float refresh_ctrl_queue_value[8];
   bool process_input;
   int patchid_queue;

   float vu_peak[8];

public:
   int CC0, PCH, patchid;
   float masterfade = 0;
   halfrate_stereo *halfbandA, *halfbandB, *halfbandIN;
   list<SurgeVoice*> voices[2];
   Effect* fx[8];
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

   list<int> holdbuffer[2];
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
