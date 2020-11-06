/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#pragma once
#include "SurgeStorage.h"
#include "SurgeVoice.h"
#include "effect/Effect.h"
#include "BiquadFilter.h"
#include "UserInteractions.h"

struct QuadFilterChainState;

#include <list>
#include <utility>
#include <atomic>
#include <cstdio>

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
#elif TARGET_JUCE
class JUCEPluginLayerProxy;
using PluginLayer = JUCEPluginLayerProxy;
#elif TARGET_HEADLESS
class HeadlessPluginLayerProxy;
using PluginLayer = HeadlessPluginLayerProxy;
#else
class PluginLayer;
#endif

struct timedata
{
   double ppqPos, tempo;
   int timeSigNumerator = 4, timeSigDenominator = 4;
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
   float sceneout alignas(16)[n_scenes][N_OUTPUTS][BLOCK_SIZE_OS]; // this is blocksize_os but has been downsampled by the end of process into block_size

   float input alignas(16)[N_INPUTS][BLOCK_SIZE];
   timedata time_data;
   bool audio_processing_active;

   // aligned stuff
   SurgeStorage storage alignas(16);
   lipol_ps FX1 alignas(16),   // TODO: FIX SCENE ASSUMPTION
            FX2 alignas(16),   // TODO: FIX SCENE ASSUMPTION
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
   void updateHighLowKeys(int scene);
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

   void resetStateFromTimeData();
   void processControl();
   void processThreadunsafeOperations();
   bool loadFx(bool initp, bool force_reload_all);
   bool loadOscalgos();
   bool load_fx_needed;

   // We have to push this onto the audio thread so have an enqueue and so on
   enum FXReorderMode { NONE, SWAP, COPY, MOVE };
   void reorderFx( int source, int target, FXReorderMode m  ); // This is safe to call from the UI thread since it just edits the sync

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
   unsigned int voices_usedby[2][MAX_VOICES]; // 0 indicates no user, 1 is scene A & 2 is scene B   // TODO: FIX SCENE ASSUMPTION!

public:

   /*
    * For more on this IFDEF see the comment in SurgeSynthesizerIDManagement.cpp
    */
#if TARGET_VST3 || TARGET_HEADLESS
#define PLUGIN_ID_AND_INDEX_ARE_DISTINCT 1
#endif

   struct ID {
      int getDawSideIndex() const { return dawindex; }
      int getDawSideId() const {
#if PLUGIN_ID_AND_INDEX_ARE_DISTINCT
         return dawid;
#else
         return dawindex;
#endif
      }
      int getSynthSideId() const { return synthid; }

      std::string toString() const {
         std::ostringstream oss;
         oss << "ID[ dawidx=" << dawindex
#if PLUGIN_ID_AND_INDEX_ARE_DISTINCT
             << ", dawid=" << dawid
#endif
             << " synthid=" << synthid << " ]";
         return oss.str();
      }

      bool operator==( const ID &other ) const {
         return dawindex == other.dawindex
#if PLUGIN_ID_AND_INDEX_ARE_DISTINCT
                && dawid == other.dawid
#endif
                && synthid == other.synthid;
      }
      bool operator!=( const ID &other ) const {
         return ! ( *this == other );
      }

      friend std::ostream &operator<<(std::ostream &os, const ID &id ) {
         os << id.toString();
         return os;
      }

   private:
      int dawindex = -1,
#if PLUGIN_ID_AND_INDEX_ARE_DISTINCT
          dawid = -1,
#endif
          synthid = -1;
      friend SurgeSynthesizer;
   };

#if PLUGIN_ID_AND_INDEX_ARE_DISTINCT
   bool fromDAWSideId( int i, ID &q );
#endif
   bool fromDAWSideIndex( int i, ID &q );
   bool fromSynthSideId( int i, ID &q );
   bool fromSynthSideIdWithGuiOffset( int i, int start_paramtags, int start_metacontrol_tag, ID &q );

   const ID idForParameter( Parameter *p ) {
      // We know this will always work
      ID i;
      fromSynthSideId(p->id, i );
      return i;
   }

   void getParameterDisplay(const ID &index, char* text)
   {
      getParameterDisplay(index.getSynthSideId(), text);
   }
   void getParameterDisplay(const ID &index, char* text, float x)
   {
      getParameterDisplay(index.getSynthSideId(), text);
   }
   void getParameterDisplayAlt(const ID &index, char* text)
   {
      getParameterDisplayAlt(index.getSynthSideId(), text);
   }
   void getParameterName(const ID &index, char* text)
   {
      getParameterName( index.getSynthSideId(), text );
   }
   void getParameterMeta(const ID &index, parametermeta& pm)
   {
      getParameterMeta(index.getSynthSideId(), pm );
   }
   void getParameterNameW(const ID &index, wchar_t* ptr)
   {
      getParameterNameW(index.getSynthSideId(), ptr);
   }
   void getParameterShortNameW(const ID &index, wchar_t* ptr)
   {
      getParameterShortNameW(index.getSynthSideId(), ptr );
   }
   void getParameterUnitW(const ID &index, wchar_t* ptr)
   {
      getParameterUnitW(index.getSynthSideId(), ptr);
   }
   void getParameterStringW(const ID &index, float value, wchar_t* ptr)
   {
      getParameterStringW(index.getSynthSideId(), value, ptr );
   }
   float getParameter01( const ID &index )
   {
      return getParameter01(index.getSynthSideId());
   }
   bool setParameter01( const ID &index, float value, bool external = false, bool force_integer = false )
   {
      return setParameter01( index.getSynthSideId(), value, external, force_integer );
   }

   float normalizedToValue( const ID &index, float val )
   {
      return normalizedToValue( index.getSynthSideId(), val );
   }

   float valueToNormalized( const ID &index, float val )
   {
      return valueToNormalized( index.getSynthSideId(), val );
   }

   void sendParameterAutomation( const ID &index, float val )
   {
      sendParameterAutomation( index.getSynthSideId(),  val);
   }


private:
   bool setParameter01(long index, float value, bool external = false, bool force_integer = false);
   void sendParameterAutomation(long index, float value);
   float getParameter01(long index);
   float getParameter(long index);
   float normalizedToValue(long parameterIndex, float value);
   float valueToNormalized(long parameterIndex, float value);
   void getParameterDisplay(long index, char* text);
   void getParameterDisplay(long index, char* text, float x);
   void getParameterDisplayAlt(long index, char* text);
   void getParameterName(long index, char* text);
   void getParameterMeta(long index, parametermeta& pm);
   void getParameterNameW(long index, wchar_t* ptr);
   void getParameterShortNameW(long index, wchar_t* ptr);
   void getParameterUnitW(long index, wchar_t* ptr);
   void getParameterStringW(long index, float value, wchar_t* ptr);
   //	unsigned int getParameterFlags (long index);

public:
   void updateDisplay();
   // bool setParameter (long index, float value);
   //	float getParameter (long index);
   bool isValidModulation(long ptag, modsources modsource);
   bool isActiveModulation(long ptag, modsources modsource);
   bool isBipolarModulation(modsources modsources);
   bool isModsourceUsed(modsources modsource);
   bool isModDestUsed(long moddest);
   ModulationRouting* getModRouting(long ptag, modsources modsource);
   bool setModulation(long ptag, modsources modsource, float value);
   float getModulation(long ptag, modsources modsource);
   float getModDepth(long ptag, modsources modsource);
   void clearModulation(long ptag, modsources modsource, bool clearEvenIfInvalid = false);
   void clear_osc_modulation(
       int scene, int entry); // clear the modulation routings on the algorithm-specific sliders
public:
   void loadRaw(const void* data, int size, bool preset = false);
   void loadPatch(int id);
   bool loadPatchByPath(const char* fxpPath, int categoryId, const char* name );
   void incrementPatch(bool nextPrev);
   void incrementCategory(bool nextPrev);

   void swapMetaControllers( int ct1, int ct2 );
   
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
   bool refresh_overflow = false;
   float refresh_ctrl_queue_value[8];
   bool process_input;
   int patchid_queue;
   bool has_patchid_file;
   char patchid_file[FILENAME_MAX];

   float vu_peak[8];

   void populateDawExtraState();
   
   void loadFromDawExtraState();
   
public:
   int CC0, CC32, PCH, patchid;
   float masterfade = 0;
   HalfRateFilter halfbandA, halfbandB, halfbandIN; // TODO: FIX SCENE ASSUMPTION (for halfbandA/B - use std::array)
   std::list<SurgeVoice*> voices[n_scenes];
   std::unique_ptr<Effect> fx[n_fx_slots];
   bool halt_engine = false;
   MidiChannelState channelState[16];
   bool mpeEnabled = false;
   int mpeVoices = 0;
   int mpeGlobalPitchBendRange = 0;

   bool hardclipEnabled = true;

   int current_category_id = 0;
   bool modsourceused[n_modsources];
   bool midiprogramshavechanged = false;

   bool switch_toggled_queued, release_if_latched[n_scenes], release_anyway[n_scenes];
   void setParameterSmoothed(long index, float value);
   BiquadFilter hpA, hpB; // TODO: FIX SCENE ASSUMPTION (use std::array)

   bool fx_reload[n_fx_slots];   // if true, reload new effect parameters from fxsync
   FxStorage fxsync[n_fx_slots]; // used for synchronisation of parameter init
   int fx_suspend_bitmask;

   // hold pedal stuff

   std::list<std::pair<int,int>> holdbuffer[n_scenes];
   void purgeHoldbuffer(int scene);
   quadr_osc sinus;
   int demo_counter = 0;

   QuadFilterChainState* FBQ[n_scenes];

   std::string hostProgram = "Unknown Host";
   bool activateExtraOutputs = true;
   void setupActivateExtraOutputs();

   void changeModulatorSmoothing( ControllerModulationSource::SmoothingMode m );

   // these have to be thread-safe, so keep private
private:
   
   PluginLayer* _parent = nullptr;

   void switch_toggled();

   // midicontrol-interpolators
   static const int num_controlinterpolators = 128;
   ControllerModulationSource mControlInterpolator[num_controlinterpolators];
   bool mControlInterpolatorUsed[num_controlinterpolators];

   int GetFreeControlInterpolatorIndex();
   int GetControlInterpolatorIndex(int Idx);
   void ReleaseControlInterpolator(int Idx);
   ControllerModulationSource* ControlInterpolator(int Idx);
   ControllerModulationSource* AddControlInterpolator(int Idx, bool& AlreadyExisted);
};
