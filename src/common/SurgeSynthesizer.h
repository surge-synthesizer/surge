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

#if TARGET_AU
class aulayer;
typedef aulayer PluginLayer;
#elif TARGET_VST3
class SurgeProcessor;
typedef SurgeProcessor PluginLayer;
#else
class Vst2PluginInstance;
typedef Vst2PluginInstance PluginLayer;
#endif

class SurgeSynthesizer : public AbstractSynthesizer
{
public:
   // aligned stuff
   _MM_ALIGN16 SurgeStorage storage;
   _MM_ALIGN16 lipol_ps FX1, FX2, amp, amp_mute, send[2][2];

   // methods
public:
   SurgeSynthesizer(PluginLayer* parent);
   virtual ~SurgeSynthesizer();
   void play_note(char channel, char key, char velocity, char detune);
   void release_note(char channel, char key, char velocity);
   void release_note_postholdcheck(int scene, char channel, char key, char velocity);
   void pitch_bend(char channel, int value);
   void poly_aftertouch(char channel, int key, int value);
   void channel_aftertouch(char channel, int value);
   void channel_controller(char channel, int cc, int value);
   void program_change(char channel, int value);
   void sysex(size_t size, unsigned char* data);
   void all_notes_off();
   void set_samplerate(float sr);
   int get_n_inputs()
   {
      return n_inputs;
   }
   int get_n_outputs()
   {
      return n_outputs;
   }
   int get_block_size()
   {
      return block_size;
   }
   int getMpeMainChannel(int voiceChannel, int key);
   void process();

   PluginLayer* getParent();

   // protected:

   void onRPN(int channel, int lsbRPN, int msbRPN, int lsbValue, int msbValue);
   void onNRPN(int channel, int lsbNRPN, int msbNRPN, int lsbValue, int msbValue);

   void process_control();
   void process_threadunsafe_operations();
   bool load_fx(bool initp, bool force_reload_all);
   bool load_oscalgos();
   bool load_fx_needed;
   void play_voice(int scene, char channel, char key, char velocity, char detune);
   void release_scene(int s);
   int calc_channelmask(int channel, int key);
   void softkill_voice(int scene);
   void enforce_polylimit(int scene, int margin);
   int get_non_ur_voices(int scene);
   int get_non_released_voices(int scene);

   SurgeVoice* get_unused_voice(int scene);
   void free_voice(SurgeVoice*);
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
   bool isValidModulation(long ptag, long modsource);
   bool isActiveModulation(long ptag, long modsource);
   bool isModsourceUsed(long modsource);
   bool isModDestUsed(long moddest);
   ModulationRouting* getModRouting(long ptag, long modsource);
   bool setModulation(long ptag, long modsource, float value);
   float getModulation(long ptag, long modsource);
   float getModDepth(long ptag, long modsource);
   void clearModulation(long ptag, long modsource);
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
   void load_raw(const void* data, int size, bool preset = false);
   void load_patch(int id);
   void increment_patch(int category, int patch);

   string getUserPatchDirectory();
   string getLegacyUserPatchDirectory();

   void save_patch();
   void update_usedstate();
   void prepare_modsource_doprocess(int scenemask);
   unsigned int save_raw(void** data);
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
   void purge_holdbuffer(int scene);
   quadr_osc sinus;
   int demo_counter = 0;

   QuadFilterChainState* FBQ[2];

   // dessa måste kallas threadsafe, så låt private vara kvar
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