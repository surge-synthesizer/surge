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
#include "globals.h"
#include "Parameter.h"
#include "ModulationSource.h"
#include "Wavetable.h"
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <stdint.h>

#include "tinyxml/tinyxml.h"

#include "filesystem/import.h"

#include <fstream>
#include <iterator>
#include <functional>
#include <unordered_map>
#include <map>

#include "Tunings.h"

#if WINDOWS
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

// patch layer

const int n_oscs = 3;
const int n_lfos_voice = 6;
const int n_lfos_scene = 6;
const int n_lfos = n_lfos_voice + n_lfos_scene;
const int n_osc_params = 7;
const int n_fx_params = 12;
const int FIRipol_M = 256;
const int FIRipol_M_bits = 8;
const int FIRipol_N = 12;
const int FIRoffset = FIRipol_N >> 1;
const int FIRipolI16_N = 8;
const int FIRoffsetI16 = FIRipolI16_N >> 1;

const int n_fx_slots=8;

// XML storage fileformat revision
// 0 -> 1 new EG attack shapes (0>1, 1>2, 2>2)
// 1 -> 2 new LFO EG stages (if (decay == max) sustain = max else sustain = min
// 2 -> 3 filter subtypes added comb should default to 1 and moog to 3
// 3 -> 4 comb+/- combined into 1 filtertype (subtype 0,0->0 0,1->1 1,0->2 1,1->3 )
// 4 -> 5 stereo filterconf now have seperate pan controls
// 5 -> 6 new filter sound in v1.2 (same parameters, but different sound & changed resonance
// response).
// 6 -> 7 custom controller state now stored (in seq. recall)
// 7 -> 8 larger resonance
// range (old filters are set to subtype 1), pan2 -> width
// 8 -> 9 now 8 controls (offset ids larger
// than ctrl7 by +1), custom controllers have names (guess for pre-rev9 patches)
// 9 -> 10 added character parameter
// 10 -> 11 (1.6.2 release) added DAW Extra State
// 11 -> 12 (1.6.3 release) added new parameters to the Distortion effect
// 12 -> 13 (1.7.0 release) deactivation; sine LP/HP, sine/FM2/3 feedback extension/bipolar
// 13 -> 14 add phaser number of stages parameter
// 13 -> 14 add ability to configure vocoder modulator mono/sterao/L/R

const int ff_revision = 14;

extern float sinctable alignas(16)[(FIRipol_M + 1) * FIRipol_N * 2];
extern float sinctable1X alignas(16)[(FIRipol_M + 1) * FIRipol_N];
extern short sinctableI16 alignas(16)[(FIRipol_M + 1) * FIRipolI16_N];
extern float table_envrate_lpf alignas(16)[512],
             table_envrate_linear alignas(16)[512],
             table_glide_exp alignas(16)[512],
             table_glide_log alignas(16)[512];
extern float table_note_omega alignas(16)[2][512];
extern float waveshapers alignas(16)[8][1024];
extern float samplerate, samplerate_inv;
extern double dsamplerate, dsamplerate_inv;
extern double dsamplerate_os, dsamplerate_os_inv;

const int n_scene_params = 271;
const int n_global_params = 113;
const int n_global_postparams = 1;
const int n_total_params = n_global_params + 2 * n_scene_params + n_global_postparams;
const int metaparam_offset = 20480; // has to be bigger than total + 16 * 130 for fake VST3 mapping
const int n_scenes = 2;
const int n_filterunits_per_scene = 2;

enum scene_mode
{
   sm_single = 0,
   sm_split,
   sm_dual,
   sm_chsplit,
   n_scenemodes,
};

const char scene_mode_names[n_scenemodes][16] =
{
   "Single",
   "Key Split",
   "Dual",
   "Channel Split",
};

enum play_mode
{
   pm_poly = 0,
   pm_mono,
   pm_mono_st,
   pm_mono_fp,
   pm_mono_st_fp,
   pm_latch,
   n_polymodes,
};
const char play_mode_names[n_polymodes][64] =
{
   "Poly",
   "Mono",
   "Mono (Single Trigger)",
   "Mono (Fingered Portamento)",
   "Mono (Single Trigger + Fingered Portamento)",
   "Latch (Monophonic)",
};

enum porta_curve
{
    porta_log = -1,
    porta_lin = 0,
    porta_exp = 1,
};

enum deform_type
{
    type_1,
    type_2,
    type_3,
    n_types,
};

enum lfo_mode
{
   lm_freerun = 0,
   lm_keytrigger,
   lm_random,
   n_lfomodes,
};

const char lfo_mode_names[n_lfomodes][16] =
{
   "Freerun",
   "Keytrigger",
   "Random",
};

enum character_mode
{
   cm_warm = 0,
   cm_neutral,
   cm_bright,
   n_charactermodes,
};
const char character_names[n_charactermodes][16] =
{
   "Warm",
   "Neutral",
   "Bright",
};

enum osc_types
{
   ot_classic = 0,
   ot_sinus,
   ot_wavetable,
   ot_shnoise,
   ot_audioinput,
   ot_FM3, // it used to just FM, then the UI called it FM3, so name it this way but this order has to stick
   ot_FM2,
   ot_WT2,
   num_osctypes,
};
const char osc_type_names[num_osctypes][16] =
{
   "Classic",
   "Sine",
   "Wavetable",
   "S&H Noise",
   "Audio In",
   "FM3",
   "FM2",
   "Window",
};

const char window_names[9][16] =
{
   "Triangle",
   "Cosine",
   "Blend 1",
   "Blend 2",
   "Blend 3",
   "Sawtooth",
   "Sine",
   "Square",
   "Rectangle",
};

inline bool uses_wavetabledata(int i)
{
   switch (i)
   {
   case ot_wavetable:
   case ot_WT2:
      return true;
   }
   return false;
}

const char fxslot_names[8][NAMECHARS] =
{
   "A Insert FX 1",
   "A Insert FX 2",
   "B Insert FX 1",
   "B Insert FX 2",
   "Send FX 1",
   "Send FX 2",
   "Global FX 1",
   "Global FX 2",
};


enum fx_types
{
   fxt_off = 0,
   fxt_delay,
   fxt_reverb,
   fxt_phaser,
   fxt_rotaryspeaker,
   fxt_distortion,
   fxt_eq,
   fxt_freqshift,
   fxt_conditioner,
   fxt_chorus4,
   fxt_vocoder,
   fxt_reverb2,
   fxt_flanger,
   fxt_ringmod,
   fxt_airwindows,
   num_fxtypes,
};
const char fx_type_names[num_fxtypes][16] =
{
   "Off",
   "Delay",
   "Reverb 1",
   "Phaser",
   "Rotary",
   "Distortion",
   "EQ",
   "Freq Shift",
   "Conditioner",
   "Chorus",
   "Vocoder",
   "Reverb 2",
   "Flanger",
   "Ring Mod",
   "Airwindows",
};

enum fx_bypass
{
   fxb_all_fx = 0,
   fxb_no_sends,
   fxb_scene_fx_only,
   fxb_no_fx,
   n_fx_bypass,
};

const char fxbypass_names[n_fx_bypass][32] =
{
   "All FX",
   "No Send FX",
   "No Send And Global FX",
   "All FX Off",
};

enum fb_configuration
{
   fb_serial = 0,
   fb_serial2,
   fb_serial3,
   fb_dual,
   fb_dual2,
   fb_stereo,
   fb_ring,
   fb_wide,
   n_fb_configuration,
};

const char fbc_names[n_fb_configuration][16] =
{
   "Serial 1",
   "Serial 2",
   "Serial 3",
   "Dual 1",
   "Dual 2",
   "Stereo",
   "Ring",
   "Wide",
};

enum fm_configuration
{
   fm_off = 0,
   fm_2to1,
   fm_3to2to1,
   fm_2and3to1,
   n_fm_configuration,
};

const char fmc_names[n_fm_configuration][16] =
{
   "Off",
   "2 > 1",
   "3 > 2 > 1",
   "2 > 1 < 3",
};

enum lfotypes
{
   lt_sine = 0,
   lt_tri,
   lt_square,
   lt_ramp,
   lt_noise,
   lt_snh,
   lt_envelope,
   lt_stepseq,
   lt_mseg,
   lt_function,
   n_lfotypes
};

const char lt_names[n_lfotypes][32] =
{
   "Sine",
   "Triangle",
   "Square",
   "Sawtooth",
   "Noise",
   "Sample & Hold",
   "Envelope",
   "Step Sequencer",
   "MSEG",
   "Function (coming in 1.9!)",
};

enum fu_type
{
   fut_none = 0,
   fut_lp12,
   fut_lp24,
   fut_lpmoog,
   fut_hp12,
   fut_hp24,
   fut_bp12,
   fut_br12,
   fut_comb,
   fut_SNH,
   fut_vintageladder,
   fut_obxd_2pole,
   fut_obxd_4pole,
   fut_k35_lp,
   fut_k35_hp,
   fut_diode,
   n_fu_type,
};
const char fut_names[n_fu_type][32] =
{
   "Off",
   "Lowpass 12 dB/oct",
   "Lowpass 24 dB/oct",
   "Legacy Ladder",
   "Highpass 12 dB/oct",
   "Highpass 24 dB/oct",
   "Bandpass",
   "Notch",
   "Comb",
   "Sample & Hold",
   "Vintage Ladder",
   "OB-Xd 12 dB/oct",
   "OB-Xd 24 dB/oct",
   "Sallen-Key Lowpass",
   "Sallen-Key Highpass",
   "Diode Ladder",
};

const char fut_bp_subtypes[6][32] =
{
   "Clean 12 dB/oct",
   "Driven 12 dB/oct",
   "Smooth 12 dB/oct",
   "Clean 24 dB/oct",
   "Driven 24 dB/oct",
   "Smooth 24 dB/oct",
};

const char fut_br_subtypes[4][32] =
{
   "12 dB/oct",
   "12 dB/oct Mild",
   "24 dB/oct",
   "24 dB/oct Mild",
};

const char fut_comb_subtypes[4][64] =
{
   "Positive, 50% Wet",
   "Positive, 100% Wet",
   "Negative, 50% Wet",
   "Negative, 100% Wet",
};

const char fut_def_subtypes[3][32] =
{
   "Clean",
   "Driven",
   "Smooth",
};

const char fut_ldr_subtypes[4][32] =
{
   "6 dB/oct",
   "12 dB/oct",
   "18 dB/oct",
   "24 dB/oct",
};

const char fut_vintageladder_subtypes[6][32] =
{
   "Strong",
   "Strong Compensated",
   "Dampened",
   "Dampened Compensated",
};

const char fut_obxd_2p_subtypes[8][32] =
{
   "Lowpass",
   "Bandpass",
   "Highpass",
   "Notch",
   "Lowpass Pushed",
   "Bandpass Pushed",
   "Highpass Pushed",
   "Notch Pushed",
};

const char fut_obxd_4p_subtypes[4][32] =
{
   "6 dB/oct",
   "12 dB/oct",
   "18 dB/oct",
   "24 dB/oct",
};

const char fut_k35_subtypes[5][32] =
{
   "No Saturation",
   "Mild Saturation",
   "Moderate Saturation",
   "Heavy Saturation",
   "Extreme Saturation"
};

const float fut_k35_saturations[5] =
{
   0.0f,
   1.0f,
   2.0f,
   3.0f,
   4.0f
};

const char fut_diode_subtypes[1][32] =
{
   "24 dB/oct"
};

const int fut_subcount[n_fu_type] = {
   0, // fut_none
   3, // fut_lp12
   3, // fut_lp24
   4, // fut_lpmoog
   3, // fut_hp12
   3, // fut_hp24
   6, // fut_bp12
   4, // fut_br12
   4, // fut_comb
   0, // fut_SNH
   4, // fut_vintageladder
   8, // fut_obxd_2pole
   4, // fut_obxd_4pole
   5, // fut_k35_lp
   5, // fut_k35_hp
   0  // fut_diode
};

enum fu_subtype
{
   st_SVF = 0,
   st_Rough = 1,
   st_Smooth = 2,
   st_Medium = 3, // disabled
   st_SVFBP24 = 3,
   st_RoughBP24 = 4,
   st_SmoothBP24 = 5,
   st_BR12 = 0,
   st_BR12Mild = 1,
   st_BR24 = 2,
   st_BR24Mild = 3,
};

enum ws_type
{
   wst_none = 0,
   wst_tanh,
   wst_hard,
   wst_asym,
   wst_sinus,
   wst_digi,
   n_ws_type,
};

const char wst_names[n_ws_type][16] =
{
   "Off",
   "Soft",
   "Hard",
   "Asymmetric",
   "Sine",
   "Digital",
};

enum env_mode_type
{
   emt_digital = 0,
   emt_analog,
   n_em_type,
};

const char em_names[n_em_type][16] =
{
   "Digital",
   "Analog",
};

struct MidiKeyState
{
   int keystate;
   char lastdetune;
};

struct MidiChannelState
{
   MidiKeyState keyState[128];
   int nrpn[2], nrpn_v[2];
   int rpn[2], rpn_v[2];
   int pitchBend;
   bool nrpn_last;
   bool hold;
   float pan;
   float pitchBendInSemitones;
   float pressure;
   float timbre;
};

// I have used the ordering here in SurgeGUIEditor to iterate. Be careful if tyoe or retrigger move from first/last position.
struct OscillatorStorage : public CountedSetUserData // The counted set is the wt tables
{
   Parameter type;
   Parameter pitch, octave;
   Parameter p[n_osc_params];
   Parameter keytrack, retrigger;
   Wavetable wt;
   char wavetable_display_name[256];
   void* queue_xmldata;
   int queue_type;

   virtual int getCountedSetSize()
   {
      return wt.n_tables;
   }
};

struct FilterStorage
{
   Parameter type;
   Parameter subtype; // NOTE: In SurgeSynthesizer we assume that type and subtype are adjacent in param space. See comment there.
   Parameter cutoff;
   Parameter resonance;
   Parameter envmod;
   Parameter keytrack;
};

struct WaveshaperStorage
{
   Parameter type;
   Parameter drive;
};

struct ADSRStorage
{
   Parameter a, d, s, r;
   Parameter a_s, d_s, r_s;
   Parameter mode;
};

// I have used the ordering here in CLFOGui to iterate. Be careful if rate or release move from first/last position.
struct LFOStorage
{
   Parameter rate, shape, start_phase, magnitude, deform;
   Parameter trigmode, unipolar;
   Parameter delay, hold, attack, decay, sustain, release;
};

struct FxStorage
{
   // Just a heads up if you change this please go look at fx_reorder in SurgeSorage too
   Parameter type;
   Parameter return_level;
   Parameter p[n_fx_params];
};

struct SurgeSceneStorage
{
   OscillatorStorage osc[n_oscs];
   Parameter pitch, octave;
   Parameter fm_depth, fm_switch;
   Parameter drift, noise_colour, keytrack_root;
   Parameter osc_solo;
   Parameter level_o1, level_o2, level_o3, level_noise, level_ring_12, level_ring_23, level_pfg;
   Parameter mute_o1, mute_o2, mute_o3, mute_noise, mute_ring_12, mute_ring_23; // all mute parameters must be contiguous
   Parameter solo_o1, solo_o2, solo_o3, solo_noise, solo_ring_12, solo_ring_23; // all solo parameters must be contiguous
   Parameter route_o1, route_o2, route_o3, route_noise, route_ring_12, route_ring_23;
   Parameter vca_level;
   Parameter pbrange_dn, pbrange_up;
   Parameter vca_velsense;
   Parameter polymode;
   Parameter portamento;
   Parameter volume, pan, width;
   Parameter send_level[2];

   FilterStorage filterunit[2];
   Parameter f2_cutoff_is_offset, f2_link_resonance;
   WaveshaperStorage wsunit;
   ADSRStorage adsr[2];
   LFOStorage lfo[n_lfos];
   Parameter feedback, filterblock_configuration, filter_balance;
   Parameter lowcut;

   std::vector<ModulationRouting> modulation_scene, modulation_voice;
   std::vector<ModulationSource*> modsources;

   bool modsource_doprocess[n_modsources];
};

const int n_stepseqsteps = 16;
struct StepSequencerStorage
{
   float steps[n_stepseqsteps];
   int loop_start, loop_end;
   float shuffle;
   uint64_t trigmask;
};

const int max_msegs = 128;
struct MSEGStorage {
   struct segment {
      float duration;
      float dragDuration; // Snap Mode Helper
      float v0;
      float dragv0; // in snap mode, this is the location we are dragged to. It is just convenience storage.
      float nv1; // this is the v0 of the neighbor and is here just for convenience. MSEGModulationHelper::rebuildCache will set it
      float dragv1; // only used in the endpoint
      float cpduration, cpv, dragcpv, dragcpratio = 0.5;
      bool  useDeform = true;
      bool  invertDeform = false;
      enum Type {
         LINEAR = 1,
         QUAD_BEZIER,
         SCURVE,
         SINE,
         STAIRS,
         BROWNIAN,
         SQUARE,
         TRIANGLE,
         HOLD,
         SAWTOOTH,
         RESERVED,  // used to be Spike, but it broke some MSEG model constraints, so we ditched it - can add a different curve type later on!
         BUMP,
         SMOOTH_STAIRS,
      } type;
   };

   // These values are streamed so please don't change the integer values
   enum EndpointMode {
      LOCKED = 1,
      FREE = 2
   } endpointMode = FREE;

   // These values are streamed so please don't change the integer values
   enum EditMode {
       ENVELOPE = 0,    // no constraints on horizontal time axis
       LFO = 1,         // MSEG editing is constrained to just one phase unit (0 ... 1), useful for single cycle waveform editing
   } editMode = ENVELOPE;

   // These values are streamed so please don't change the integer values
   enum LoopMode {
      ONESHOT = 1,      // Play the MSEG front to back and then output the final value
      LOOP = 2,         // Play the MSEG front to loop end and then return to loop start
      GATED_LOOP = 3    // Play the MSEG front to loop end, then return to loop start, but if at any time
                        // a note off is generated, jump to loop end at current value and progress to end once
   } loopMode = LOOP;

   int loop_start = -1, loop_end = -1; // -1 signifies the entire MSEG in this context

   int n_activeSegments = 0;
   std::array<segment, max_msegs> segments;

   // These are calculated values which derive from the segment which we cache for efficiency.
   // If you edit the segments then MSEGModulationHelper::rebuildCache can rebuild them
   float totalDuration;
   std::array<float, max_msegs> segmentStart, segmentEnd;
   float durationToLoopEnd;
   float durationLoopStartToLoopEnd;

   static constexpr float minimumDuration = 0.0;
};

struct FormulaModulatorStorage { // Currently an unused placeholder
};

/*
** There are a collection of things we want your DAW to save about your particular instance
** but don't want saved in your patch. So have this extra structure in the patch which we
** can activate/populate from the DAW hosts. See #915
*/
struct DAWExtraStateStorage
{
   bool isPopulated = false;

   /*
    * Here's the prescription to add something to the editor state
    *
    * 1. Add it here with a reasonable default.
    * 2. In the SurgeGUIEditor Constructor, read off the value
    * 3. In SurgeGUIEditor::populateDawExtraState write it
    * 4. In SurgeGUIEditor::loadDawExtraState read it (this will probably be pretty similar to
    *    the constructor code in step 4, but this is the step when restoring, as opposed to creating
    *    an object).
    * 5. In SurgePatch load/save XML write and read it
    *
    * Then the state will survive create/destroy and save/restore
    */
   struct EditorState
   {
      int instanceZoomFactor = -1;
      int current_scene = 0;
      int current_fx = 0;
      int current_osc[n_scenes] = {0};
      modsources modsource = ms_lfo1, modsource_editor[n_scenes] = {ms_lfo1, ms_lfo1};
      bool isMSEGOpen = false;
   } editor;


   bool mpeEnabled = false;
   int mpePitchBendRange = -1;

   bool hasTuning = false;
   std::string tuningContents = "";

   bool hasMapping = false;
   std::string mappingContents = "";

   std::unordered_map<int, int> midictrl_map; // param -> midictrl
   std::unordered_map<int, int> customcontrol_map; // custom controller number -> midicontrol
};


struct PatchTuningStorage
{
   bool tuningStoredInPatch = false;
   std::string tuningContents = "";
   std::string mappingContents = "";
};

class SurgeStorage;

class SurgePatch
{
public:
   SurgePatch(SurgeStorage* storage);
   ~SurgePatch();
   void init_default_values();
   void update_controls(bool init = false, void* init_osc = 0, bool from_stream = false);
   void do_morph();
   void copy_scenedata(pdata*, int scene);
   void copy_globaldata(pdata*);

   // load/save
   // void load_xml();
   // void save_xml();
   void load_xml(const void* data, int size, bool preset);
   unsigned int save_xml(void** data);
   unsigned int save_RIFF(void** data);

   // Factor these so the LFO Preset Mechanism can use them also
   void msegToXMLElement( MSEGStorage *ms, TiXmlElement &parent ) const;
   void msegFromXMLElement( MSEGStorage *ms, TiXmlElement *parent ) const;
   void stepSeqToXmlElement( StepSequencerStorage *ss, TiXmlElement &parent, bool streamMask ) const;
   void stepSeqFromXmlElement( StepSequencerStorage *ss, TiXmlElement *parent ) const;

   void load_patch(const void* data, int size, bool preset);
   unsigned int save_patch(void** data);

   // data
   SurgeSceneStorage scene[n_scenes], morphscene;
   FxStorage fx[n_fx_slots];
   // char name[NAMECHARS];
   int scene_start[n_scenes], scene_size;
   Parameter scene_active, scenemode, scenemorph, splitpoint;  // streaming name for splitpoint is splitkey (due to legacy)
   Parameter volume;
   Parameter polylimit;
   Parameter fx_bypass, fx_disable;
   Parameter character;

   StepSequencerStorage stepsequences[n_scenes][n_lfos];
   MSEGStorage msegs[n_scenes][n_lfos];
   FormulaModulatorStorage formulamods[n_scenes][n_lfos];

   PatchTuningStorage patchTuning;
   DAWExtraStateStorage dawExtraState;

   std::vector<Parameter*> param_ptr;
   std::vector<int> easy_params_id;

   std::vector<ModulationRouting> modulation_global;
   pdata scenedata[n_scenes][n_scene_params];
   pdata globaldata[n_global_params];
   void* patchptr;
   SurgeStorage* storage;

   // metadata
   std::string name, category, author, comment;
   // metaparameters
   char CustomControllerLabel[n_customcontrollers][16];

   int streamingRevision;
   int currentSynthStreamingRevision;
};

struct Patch
{
   std::string name;
   fs::path path;
   int category;
   int order;
   bool fav;
};

struct PatchCategory
{
   std::string name;
   int order;
   std::vector<PatchCategory> children;
   bool isRoot;

   int internalid;
   int numberOfPatchesInCatgory;
   int numberOfPatchesInCategoryAndChildren;
};

enum surge_copysource
{
   cp_off = 0,
   cp_scene,
   cp_osc,
   cp_lfo,
   cp_oscmod,
   n_copysources,
};

/* STORAGE layer			*/

class alignas(16) SurgeStorage
{
public:
   float audio_in alignas(16)[2][BLOCK_SIZE_OS];
   float audio_in_nonOS alignas(16)[2][BLOCK_SIZE];
   float audio_otherscene alignas(16)[2][BLOCK_SIZE_OS]; // this will be a pointer to an aligned 2 x BLOCK_SIZE_OS array
   //	float sincoffset alignas(16)[(FIRipol_M)*FIRipol_N];	// deprecated


   SurgeStorage(std::string suppliedDataPath="");
   float table_pitch alignas(16)[512];
   float table_pitch_inv alignas(16)[512];
   float table_note_omega alignas(16)[2][512];
   float table_pitch_ignoring_tuning alignas(16)[512];
   float table_pitch_inv_ignoring_tuning alignas(16)[512];
   float table_note_omega_ignoring_tuning alignas(16)[2][512];

   ~SurgeStorage();

   std::unique_ptr<SurgePatch> _patch;

   SurgePatch& getPatch();

   float pitch_bend;

   float vu_falloff;
   float temposyncratio, temposyncratio_inv; // 1.f is 120 BPM
   double songpos;
   void init_tables();
   float nyquist_pitch;
   int last_key[2]; // TODO: FIX SCENE ASSUMPTION?
   TiXmlElement* getSnapshotSection(const char* name);
   void load_midi_controllers();
   void save_midi_controllers();
   void save_snapshots();
   int controllers[n_customcontrollers];
   float poly_aftertouch[2][128];  // TODO: FIX SCENE ASSUMPTION?
   float modsource_vu[n_modsources];
   void refresh_wtlist();
   void refresh_wtlistAddDir(bool userDir, std::string subdir);
   void refresh_patchlist();
   void refreshPatchlistAddDir(bool userDir, std::string subdir);

   void refreshPatchOrWTListAddDir(bool userDir,
                                   std::string subdir,
                                   std::function<bool(std::string)> filterOp,
                                   std::vector<Patch>& items,
                                   std::vector<PatchCategory>& categories);

   void perform_queued_wtloads();

   void load_wt(int id, Wavetable* wt, OscillatorStorage *);
   void load_wt(std::string filename, Wavetable* wt, OscillatorStorage *);
   bool load_wt_wt(std::string filename, Wavetable* wt);
   // void load_wt_wav(std::string filename, Wavetable* wt);
   void load_wt_wav_portable(std::string filename, Wavetable *wt);
   void export_wt_wav_portable(std::string fbase, Wavetable *wt);
   void clipboard_copy(int type, int scene, int entry);
   void clipboard_paste(int type, int scene, int entry);
   int get_clipboard_type();

   int getAdjacentWaveTable(int id, bool nextPrev);

   // The in-memory patch database.
   std::vector<Patch> patch_list;
   std::vector<PatchCategory> patch_category;
   int firstThirdPartyCategory;
   int firstUserCategory;
   std::vector<int> patchOrdering;
   std::vector<int> patchCategoryOrdering;

   // The in-memory wavetable database.
   std::vector<Patch> wt_list;
   std::vector<PatchCategory> wt_category;
   int firstThirdPartyWTCategory;
   int firstUserWTCategory;
   std::vector<int> wtOrdering;
   std::vector<int> wtCategoryOrdering;

   std::string wtpath;
   std::string datapath;
   std::string userDataPath;
   std::string userDefaultFilePath;
   std::string userFXPath;

   std::string userMidiMappingsPath;
   std::map<std::string, TiXmlDocument> userMidiMappingsXMLByName;
   void rescanUserMidiMappings();
   void loadMidiMappingByName( std::string name );
   void storeMidiMappingToName( std::string name );

   // float table_sin[512],table_sin_offset[512];
   std::mutex waveTableDataMutex;
   std::recursive_mutex modRoutingMutex;
   Wavetable WindowWT;

   float note_to_pitch(float x);
   float note_to_pitch_inv(float x);
   float note_to_pitch_ignoring_tuning(float x);
   float note_to_pitch_inv_ignoring_tuning(float x);
   inline float note_to_pitch_tuningctr(float x)
   {
       return note_to_pitch(x + scaleConstantNote() ) * scaleConstantPitchInv();
   }
   inline float note_to_pitch_inv_tuningctr(float x)
   {
       return note_to_pitch_inv( x + scaleConstantNote() ) * scaleConstantPitch();
   }

   void note_to_omega(float, float&, float&);
   void note_to_omega_ignoring_tuning(float, float&, float&);

   bool retuneToScale(const Tunings::Scale& s);
   bool retuneToStandardTuning() { init_tables(); return true; }

   bool remapToKeyboard(const Tunings::KeyboardMapping &k);
   bool remapToStandardKeyboard();
   inline int scaleConstantNote() { return currentMapping.tuningConstantNote; }
   inline float scaleConstantPitch() { return tuningPitch; }
   inline float scaleConstantPitchInv() { return tuningPitchInv; } // Obviously that's the inverse of the above

   Tunings::Scale currentScale;
   bool isStandardTuning;

   Tunings::KeyboardMapping currentMapping;
   bool isStandardMapping = true;
   float tuningPitch = 32.0f, tuningPitchInv = 0.03125f;

   ControllerModulationSource::SmoothingMode smoothingMode = ControllerModulationSource::SmoothingMode::LEGACY;
   ControllerModulationSource::SmoothingMode pitchSmoothingMode =
       ControllerModulationSource::SmoothingMode::LEGACY;
   float mpePitchBendRange = -1.0f;

   std::atomic<int> otherscene_clients;

   std::unordered_map<int, std::string> helpURL_controlgroup;
   std::unordered_map<std::string, std::string> helpURL_paramidentifier;
   std::unordered_map<std::string, std::string> helpURL_specials;
   // Alterhately make this unordered and provide a hash
   std::map<std::pair<std::string,int>, std::string> helpURL_paramidentifier_typespecialized;

private:
   TiXmlDocument snapshotloader;
   std::vector<Parameter> clipboard_p;
   int clipboard_type;
   StepSequencerStorage clipboard_stepsequences[n_lfos];
   MSEGStorage clipboard_msegs[n_lfos];
   std::vector<ModulationRouting> clipboard_modulation_scene, clipboard_modulation_voice;
   Wavetable clipboard_wt[n_oscs];

#if TARGET_LV2
public:
   // whether to skip loading, desired while exporting manifests
   static bool skipLoadWtAndPatch;
#endif
};

float db_to_linear(float);
float lookup_waveshape(int, float);
float lookup_waveshape_warp(int, float);
float envelope_rate_lpf(float);
float envelope_rate_linear(float);
float envelope_rate_linear_nowrap(float);
float glide_log(float);
float glide_exp(float);

namespace Surge
{
namespace Storage
{
bool isValidName(const std::string &name);
// is this really not in stdlib?
bool isValidUTF8( const std::string &testThis);

std::string findReplaceSubstring(std::string &source, const std::string &from, const std::string &to);
std::string makeStringVertical(std::string &source);

std::string appendDirectory( const std::string &root, const std::string &path1 );
std::string appendDirectory( const std::string &root, const std::string &path1, const std::string &path2 );
std::string appendDirectory( const std::string &root, const std::string &path1, const std::string &path2, const std::string &path3 );
}
}

/*
** ToElement does a this && check to check nulls. (As does ToDocument and so on).
** gcc -O3 on linux optimizes that away giving crashes. So do this instead
** See github issue #469
*/
#define TINYXML_SAFE_TO_ELEMENT(expr) ((expr)?(expr)->ToElement():NULL)
