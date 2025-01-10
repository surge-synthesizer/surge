/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#ifndef SURGE_SRC_COMMON_SURGESTORAGE_H
#define SURGE_SRC_COMMON_SURGESTORAGE_H
#include "globals.h"
#include "Parameter.h"
#include "ModulationSource.h"
#include "Wavetable.h"

#include "tinyxml/tinyxml.h"
#include "filesystem/import.h"
#include "sst/cpputils.h"

#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <functional>
#include <unordered_map>
#include <map>
#include <utility>
#include <type_traits>
#include <random>
#include <chrono>

#include "Tunings.h"
#include "PatchDB.h"
#include <unordered_set>
#include "UserDefaults.h"

/*
 * Porting to c++20 and hit this a year or two from now? Check out the fix
 * we put in in 680e3e916a112f81847033347595616c8af76d99 to understand the bug
 */
static_assert(std::is_same<std::string, decltype(std::declval<fs::path>().u8string())>::value,
              "Please enable u8 string macros");

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
const int max_lfo_indices = 8;
const int n_osc_params = 7;
const int n_egs = 2;
const int n_fx_params = 12;
const int n_fx_slots = 16;
const int n_fx_chains = 4;
const int n_fx_per_chain = 4;
const int n_send_slots = 4;
const int FIRipol_M = 256;
const int FIRipol_M_bits = 8;
const int FIRipol_N = 12;
const int FIRoffset = FIRipol_N >> 1;
const int FIRipolI16_N = 8;
const int FIRoffsetI16 = FIRipolI16_N >> 1;

// clang-format off
// XML file format revision history
// 0 -> 1                        new filter/amp EG attack shapes (0 -> 1, 1 -> 2, 2 -> 2)
// 1 -> 2                        new LFO EG stages (if (decay == max) sustain = max else sustain = min)
// 2 -> 3                        filter subtypes added: Comb defaults to 1, Legacy Ladder defaults to 3
// 3 -> 4                        Comb+ and Comb- are now combined into one filter type
//                               (type, subtype: +, 0 -> 0 | +, 1 -> 1 | -, 0 -> 2 | -, 1 -> 3)
// 4 -> 5                        Stereo filter configuration has separate pan controls now
// 5 -> 6                        (1.2.0 release) filter resonance response changed (same parameters, different sound)
// 6 -> 7                        custom controller state now stored in DAW recall
// 7 -> 8                        larger resonance range (old filters are set to subtype 1)
//                               Pan 2 -> Width
// 8 -> 9                        macros extended to 8 (offset IDs larger than ctrl7 by 1)
//                               macros can now be named (guess the name for older patches)
// 9 -> 10                       added Character parameter
// 10 -> 11 (1.6.2 release)      added DAW extra state
// 11 -> 12 (1.6.3 release)      added new parameters to Distortion effect
// 12 -> 13 (1.7.0 release)      added slider deactivation facility
//                               Sine LP/HP filters
//                               Sine/FM2/FM3 feedback extension/bipolar
// 13 -> 14 (1.8.0 nightlies)    add Phaser Stages/Center/Spread parameters
//                               add ability to configure Vocoder modulator input (monosum/L/R/stereo)
//                               add comb filter tuning and compatibility block
// 14 -> 15 (1.8.0 release)      the great remapping of filter types (GitHub issue #3006, PR #3329)
// 15 -> 16 (1.9.0 release)      oscillator retrigger is consistent now (GitHub issue #3171, PR #3785)
//                               added tuningApplicationMode to patch
//                               added disable toggle to Low/High Cut filters in various effects
//                               added disable toggle to Phaser Rate, and different waveform options
// 16 -> 17 (XT 1.0 release)     added continuous morph toggle to Window oscillator
//                               added (a lot of) new waveshapers
//                               added two additional FX slots to all FX chains
//                               added new Conditioner parameter (Side Low Cut)
//                               read Global Volume from the patch (it was always stored just never read)
// 17 -> 18 (XT 1.1 nightlies)   added clipping options to Delay Feedback parameter (via deform)
//                               added Tone parameter to Phaser effect
// 18 -> 19 (XT 1.1 nightlies)   added String deform options (interpolation, bipolar Decay params, Stiffness options)
//                               added Extend to Delay Feedback parameter (allows negative delay)
// 19 -> 20 (XT 1.1 release)     added voice envelope mode, but super late so don't break 19
// 20 -> 21 (XT 1.2 nightlies)   added absolutable mode for Combulator Offset 1/2 (to match the behavior of Center parameter)
//                               added oddsound_as_mts_main
// 21 -> 22 (XT 1.3 nighlies)    added new ring modulator modes in the mixer
//                               added Bonsai distortion effect
//                               changed MIDI mapping behavior
//                               added capability to deactivate scenes
//                               added Vintage FM feedback mode to FM2, FM3 and Sine oscillator types
//                               added extend mode to Delay Crossfeed and Mod Depth parameters
// 22 -> 23 (XT 1.3.2 release)   added storing of Tempo parameter to the patch (will be loaded in Standalone only if option enabled)
//                               added output filter to the Ensemble effect
//                               added deform option for Release parameter of Filter/Amp EG, which only produces an open gate for the release stage
// 23 -> 24 (XT 1.3.3 nightlies) added actually functioning extend mode to FM2 oscillator's M1/2 Offset parameter
//                                     (old patches load with extend disabled even if they had it enabled)
// 24 -> 25 (XT 1.3.4 nightlies) added storing of Wavetable Editor window state
// 25 -> 26 (XT 1.4.* nightlies) added WT Deform for new WT features
// clang-format on

const int ff_revision = 26;

const int n_scene_params = 273;
const int n_global_params = 11 + n_fx_slots * (n_fx_params + 1); // each param plus a type
const int n_global_postparams = 1;
const int n_total_params = n_global_params + 2 * n_scene_params + n_global_postparams;
const int n_scenes = 2;
const int n_filterunits_per_scene = 2;
const int n_max_filter_subtypes = 16;

enum scene_mode
{
    sm_single = 0,
    sm_split,
    sm_dual,
    sm_chsplit,

    n_scene_modes,
};

const char scene_mode_names[n_scene_modes][16] = {
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

    n_play_modes,
};
const char play_mode_names[n_play_modes][64] = {
    "Poly",
    "Mono",
    "Mono (Single Trigger)",
    "Mono (Fingered Portamento)",
    "Mono (Single Trigger & Fingered Portamento)",
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
    type_4,

    n_deform_types,
};

enum NoiseColorChannels
{
    STEREO = 0,
    MONO = 1
};

enum CombinatorMode
{
    cxm_ring = 0,
    cxm_cxor43_0,
    cxm_cxor43_1,
    cxm_cxor43_2,
    cxm_cxor43_3_legacy,
    cxm_cxor43_4_legacy,
    cxm_cxor43_3,
    cxm_cxor43_4,
    cxm_cxor93_0,
    cxm_cxor93_1,
    cxm_cxor93_2,
    cxm_cxor93_3,
    cxm_cxor93_4,

    n_cxm_modes,
};

const char combinator_mode_names[n_cxm_modes][20] = {
    "Ring Modulation", "Continuous XOR",                                         //
    "Mode 1",          "Mode 2",         "Mode 3", "Mode 4", "Mode 5", "Mode 6", // SILM 4 gradients
    "Mode 1",          "Mode 2",         "Mode 3", "Mode 4", "Mode 5",           // SILM 9 gradients
};

enum lfo_trigger_mode
{
    lm_freerun = 0,
    lm_keytrigger,
    lm_random,

    n_lfo_trigger_modes,
};

const char lfo_trigger_mode_names[n_lfo_trigger_modes][16] = {
    "Freerun",
    "Keytrigger",
    "Random",
};

enum character_mode
{
    cm_warm = 0,
    cm_neutral,
    cm_bright,

    n_character_modes,
};
const char character_names[n_character_modes][16] = {
    "Warm",
    "Neutral",
    "Bright",
};

// IDs are used in resources/data/configuration.xml
enum osc_type
{
    ot_classic = 0, // #0
    ot_sine,        // #1
    ot_wavetable,   // #2
    ot_shnoise,     // #3
    ot_audioinput,  // #4
    // used to be just FM (FM3 on GUI), so name it like this, but enum order has to stick
    ot_FM3,    // #5
    ot_FM2,    // #6
    ot_window, // #7
    ot_modern, // #8
    ot_string, // #9
    ot_twist,  // #10
    ot_alias,  // #11
    // ot_phasedist,
    // ot_chaos,
    // ot_FM4,

    n_osc_types,
};

const char osc_type_names[n_osc_types][24] = {"Classic",  "Sine",   "Wavetable", "S&H Noise",
                                              "Audio In", "FM3",    "FM2",       "Window",
                                              "Modern",   "String", "Twist",     "Alias",
                                              /*, "Phase Distortion", "Chaos", "FM4"*/};

const char osc_type_shortnames[n_osc_types][24] = {"Classic",  "Sine",   "WT",    "S&H Noise",
                                                   "Audio In", "FM3",    "FM2",   "Window",
                                                   "Modern",   "String", "Twist", "Alias",
                                                   /*, "PD", "Chaos", "FM4"*/};

const char window_names[9][16] = {
    "Triangle", "Cosine", "Blend 1", "Blend 2",   "Blend 3",
    "Sawtooth", "Sine",   "Square",  "Rectangle",
};

inline bool uses_wavetabledata(int i)
{
    switch (i)
    {
    case ot_wavetable:
    case ot_window:
        return true;
    }
    return false;
}

enum fxslot_positions
{
    fxslot_ains1 = 0,
    fxslot_ains2,
    fxslot_bins1,
    fxslot_bins2,
    fxslot_send1,
    fxslot_send2,
    fxslot_global1,
    fxslot_global2,
    fxslot_ains3,
    fxslot_ains4,
    fxslot_bins3,
    fxslot_bins4,
    fxslot_send3,
    fxslot_send4,
    fxslot_global3,
    fxslot_global4
};

// clang-format off
static int constexpr fxslot_order[n_fx_slots] =
    {fxslot_ains1,   fxslot_ains2,   fxslot_ains3,   fxslot_ains4,
     fxslot_bins1,   fxslot_bins2,   fxslot_bins3,   fxslot_bins4,
     fxslot_send1,   fxslot_send2,   fxslot_send3,   fxslot_send4,
     fxslot_global1, fxslot_global2, fxslot_global3, fxslot_global4};
// clang-format on

enum fxchains
{
    fxc_scenea = 0,
    fxc_sceneb,
    fxc_send,
    fxc_global,
};

const char fxslot_names[n_fx_slots][NAMECHARS] = {
    "A Insert FX 1", "A Insert FX 2", "B Insert FX 1", "B Insert FX 2",
    "Send FX 1",     "Send FX 2",     "Global FX 1",   "Global FX 2",
    "A Insert FX 3", "A Insert FX 4", "B Insert FX 3", "B Insert FX 4",
    "Send FX 3",     "Send FX 4",     "Global FX 3",   "Global FX 4",
};

const char fxslot_longnames[n_fx_slots][NAMECHARS] = {
    "Scene A, Insert FX Slot 1",
    "Scene A, Insert FX Slot 2",
    "Scene B, Insert FX Slot 1",
    "Scene B, Insert FX Slot 2",
    "Send FX Slot 1",
    "Send FX Slot 2",
    "Global FX Slot 1",
    "Global FX Slot 2",
    "Scene A, Insert FX Slot 3",
    "Scene A, Insert FX Slot 4",
    "Scene B, Insert FX Slot 3",
    "Scene B, Insert FX Slot 4",
    "Send FX Slot 3",
    "Send FX Slot 4",
    "Global FX Slot 3",
    "Global FX Slot 4",
};

const char fxslot_shortnames[n_fx_slots][8] = {
    "FX A1", "FX A2", "FX B1", "FX B2", "FX S1", "FX S2", "FX G1", "FX G2",
    "FX A3", "FX A4", "FX B3", "FX B4", "FX S3", "FX S4", "FX G3", "FX G4",
};
const std::string fxslot_shortoscname[n_fx_slots] = {
    "fx/a/1", "fx/a/2", "fx/b/1", "fx/b/2", "fx/send/1", "fx/send/2", "fx/global/1", "fx/global/2",
    "fx/a/3", "fx/a/4", "fx/b/3", "fx/b/4", "fx/send/3", "fx/send/4", "fx/global/3", "fx/global/4",
};

enum fx_type
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
    fxt_neuron,
    fxt_geq11,
    fxt_resonator,
    fxt_chow,
    fxt_exciter,
    fxt_ensemble,
    fxt_combulator,
    fxt_nimbus,
    fxt_tape,
    fxt_treemonster,
    fxt_waveshaper,
    fxt_mstool,
    fxt_spring_reverb,
    fxt_bonsai,
    fxt_audio_input,
    fxt_floaty_delay,

    n_fx_types,
};

const char fx_type_names[n_fx_types][32] = {"Off",
                                            "Delay",
                                            "Reverb 1",
                                            "Phaser",
                                            "Rotary Speaker",
                                            "Distortion",
                                            "EQ",
                                            "Frequency Shifter",
                                            "Conditioner",
                                            "Chorus",
                                            "Vocoder",
                                            "Reverb 2",
                                            "Flanger",
                                            "Ring Modulator",
                                            "Airwindows",
                                            "Neuron",
                                            "Graphic EQ",
                                            "Resonator",
                                            "CHOW",
                                            "Exciter",
                                            "Ensemble",
                                            "Combulator",
                                            "Nimbus",
                                            "Tape",
                                            "Treemonster",
                                            "Waveshaper",
                                            "Mid-Side Tool",
                                            "Spring Reverb",
                                            "Bonsai",
                                            "Audio Input",
                                            "Floaty Delay"};

const char fx_type_shortnames[n_fx_types][16] = {
    "Off",         "Delay",      "Reverb 1",      "Phaser",        "Rotary",     "Distortion",
    "EQ",          "Freq Shift", "Conditioner",   "Chorus",        "Vocoder",    "Reverb 2",
    "Flanger",     "Ring Mod",   "Airwindows",    "Neuron",        "Graphic EQ", "Resonator",
    "CHOW",        "Exciter",    "Ensemble",      "Combulator",    "Nimbus",     "Tape",
    "Treemonster", "Waveshaper", "Mid-Side Tool", "Spring Reverb", "Bonsai",     "Audio In",
    "Floaty Delay"};

const char fx_type_acronyms[n_fx_types][8] = {
    "OFF", "DLY",  "RV1", "PH", "ROT", "DIST", "EQ",  "FRQ", "DYN", "CH",  "VOC",
    "RV2", "FL",   "RM",  "AW", "NEU", "GEQ",  "RES", "CHW", "XCT", "ENS", "CMB",
    "NIM", "TAPE", "TM",  "WS", "M-S", "SRV",  "BON", "IN",  "FDL"};

enum fx_bypass
{
    fxb_all_fx = 0,
    fxb_no_sends,
    fxb_scene_fx_only,
    fxb_no_fx,

    n_fx_bypass,
};

const char fxbypass_names[n_fx_bypass][32] = {
    "All FX",
    "No Send FX",
    "No Send and Global FX",
    "All FX Off",
};

enum filter_config
{
    fc_serial1 = 0,
    fc_serial2,
    fc_serial3,
    fc_dual1,
    fc_dual2,
    fc_stereo,
    fc_ring,
    fc_wide,

    n_filter_configs,
};

const char fbc_names[n_filter_configs][16] = {
    "Serial 1", "Serial 2", "Serial 3", "Dual 1", "Dual 2", "Stereo", "Ring", "Wide",
};

enum fm_routing
{
    fm_off = 0,
    fm_2to1,
    fm_3to2to1,
    fm_2and3to1,

    n_fm_routings,
};

const char fmr_names[n_fm_routings][16] = {
    "Off",
    "2 > 1",
    "3 > 2 > 1",
    "2 > 1 < 3",
};

enum lfo_type
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
    lt_formula,

    n_lfo_types,
};

const char lt_names[n_lfo_types][32] = {
    "Sine",          "Triangle", "Square",         "Sawtooth", "Noise",
    "Sample & Hold", "Envelope", "Step Sequencer", "MSEG",     "Formula",
};

const int lt_num_deforms[n_lfo_types] = {
    3, // lt_sine
    3, // lt_tri
    0, // lt_square
    3, // lt_ramp
    0, // lt_noise
    2, // lt_snh
    3, // lt_envelope
    2, // lt_stepseq
    0, // lt_mseg
    3, // lt_formula
};

/*
 * With 1.8 for compactness all the filter names and stuff went to a separate header
 */
#include "FilterConfiguration.h"

enum env_mode
{
    emt_digital = 0,
    emt_analog,

    n_env_modes,
};

const char em_names[n_env_modes][16] = {
    "Digital",
    "Analog",
};

enum adsr_purpose
{
    adsr_ampeg = 0,
    adsr_filteg
};

/*
 * How does the sustain pedal work in mono mode? Current modes for this are:
 *
 * HOLD_ALL_NOTES (the default). If you release a note with the pedal down
 * it does not release
 *
 * RELEASE_IF_OTHERS_HELD. If you release a note, and no other notes are down,
 * do not release. But if you release and another note is down, return to that
 * note (basically allow sustain pedal trills).
 */
enum MonoPedalMode
{
    HOLD_ALL_NOTES,
    RELEASE_IF_OTHERS_HELD
};

enum MonoVoicePriorityMode
{
    NOTE_ON_LATEST_RETRIGGER_HIGHEST, // The legacy mode for 1.7.1 and earlier
    ALWAYS_LATEST,                    // Could also be called "NOTE_ON_LATEST_RETRIGGER_LATEST"
    ALWAYS_HIGHEST,
    ALWAYS_LOWEST,
};

enum MonoVoiceEnvelopeMode
{
    RESTART_FROM_ZERO,
    RESTART_FROM_LATEST
};

enum PolyVoiceRepeatedKeyMode
{
    NEW_VOICE_EVERY_NOTEON,
    ONE_VOICE_PER_KEY, // aka "piano mode"
};

struct MidiKeyState
{
    int keystate;
    char lastdetune;
    int32_t lastNoteIdForKey;
    int64_t voiceOrder;
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

// I have used the ordering here in SurgeGUIEditor to iterate. Be careful if type or retrigger move
// from first/last position.
struct OscillatorStorage : public CountedSetUserData // The counted set is the wavetables
{
    Parameter type;
    Parameter pitch, octave;
    Parameter p[n_osc_params];
    Parameter keytrack, retrigger;
    Wavetable wt;

    std::string wavetable_display_name;
    std::string wavetable_formula = "";
    int wavetable_formula_res_base = 5, // 32 * 2^this
        wavetable_formula_nframes = 10;

    void *queue_xmldata;
    int queue_type;

    struct ExtraConfigurationData
    {
        static constexpr size_t max_config = 64;
        int nData = 0;
        float data[max_config];
    } extraConfig;

    virtual int getCountedSetSize() const { return wt.n_tables; }
};

struct FilterStorage
{
    Parameter type;    // NOTE: In SurgeSynthesizer we assume that type and subtype are
    Parameter subtype; // adjacent in param space. See comment there.
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

// I have used the ordering here in CLFOGui to iterate.
// Be careful if Rate or LFO EG Release move from first/last position!
struct LFOStorage
{
    Parameter rate, shape, start_phase, magnitude, deform;
    Parameter trigmode, unipolar;
    Parameter delay, hold, attack, decay, sustain, release;

    enum LFOExtraOutputAmplitude
    {
        UNSCALED,
        SCALED
    } lfoExtraAmplitude{UNSCALED};
};

struct FxStorage
{
    // Just a heads up: if you change this, please go look at reorderFx in SurgeSynthesizer too!
    FxStorage(fxslot_positions slot) : fxslot(slot) {}
    FxStorage() = delete;

    // Note that some clients assume that &(FxStorage) == &(FxStorage->type) and the
    // param range is from &FXStorage -> &FXStorage->p[end]. This used to be true but
    // is too limiting an assumption. We are fixing those clients now (rack still
    // left to do) but if you add members please add them to the *end* of the param stack
    // until we get em all
    Parameter type;
    Parameter return_level;
    Parameter p[n_fx_params];

    // like this one!
    fxslot_positions fxslot;
};

struct SurgeSceneStorage
{
    OscillatorStorage osc[n_oscs];

    // If you change the start and end points of this param list the iteration in the
    // dump to HTML will break.
    Parameter pitch, octave;
    Parameter fm_depth, fm_switch;
    Parameter drift, noise_colour, keytrack_root;
    Parameter osc_solo;
    // TODO: in XT2 add osc4-6 params after osc3, not at the end!
    Parameter level_o1, level_o2, level_o3, level_noise, level_ring_12, level_ring_23, level_pfg;
    // all mute and solo parameters must be contiguous!
    Parameter mute_o1, mute_o2, mute_o3, mute_noise, mute_ring_12, mute_ring_23;
    Parameter solo_o1, solo_o2, solo_o3, solo_noise, solo_ring_12, solo_ring_23;
    Parameter route_o1, route_o2, route_o3, route_noise, route_ring_12, route_ring_23;
    Parameter vca_level;
    Parameter pbrange_dn, pbrange_up;
    Parameter vca_velsense;
    Parameter polymode;
    Parameter portamento;
    Parameter volume, pan, width;
    Parameter send_level[n_send_slots];

    FilterStorage filterunit[2];
    Parameter f2_cutoff_is_offset, f2_link_resonance;
    WaveshaperStorage wsunit;
    ADSRStorage adsr[2];
    LFOStorage lfo[n_lfos];
    Parameter feedback, filterblock_configuration, filter_balance;
    Parameter lowcut;

    std::vector<ModulationRouting> modulation_scene, modulation_voice;
    std::vector<ModulationSource *> modsources;

    bool modsource_doprocess[n_modsources];

    MonoVoicePriorityMode monoVoicePriorityMode = ALWAYS_LATEST;
    MonoVoiceEnvelopeMode monoVoiceEnvelopeMode = RESTART_FROM_ZERO;
    PolyVoiceRepeatedKeyMode polyVoiceRepeatedKeyMode = NEW_VOICE_EVERY_NOTEON;
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
struct MSEGStorage
{
    struct segment // If you add something here fix blankAllSegments in MSEGModulationHelper
    {
        float duration;
        float dragDuration; // Snap mode helper
        float v0;
        float dragv0; // In snap mode, this is the location we are dragged to.
                      // It is just convenience storage.
        float nv1;    // This is the v0 of the neighbor and is here just for convenience.
                      // MSEGModulationHelper::rebuildCache will set it
        float dragv1; // Only used in the endpoint
        float cpduration, cpv, dragcpv, dragcpratio = 0.5;

        bool useDeform = true, invertDeform = false;
        bool retriggerFEG = false, retriggerAEG = false;

        enum Type
        {
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
            RESERVED, // used to be Spike, but it broke some MSEG model constraints,
                      // so we ditched it - can add a different curve type later on!
            BUMP,
            SMOOTH_STAIRS,
        } type;
    };

    // These values are streamed so please don't change the integer values!
    enum EndpointMode
    {
        LOCKED = 1,
        FREE = 2
    } endpointMode = FREE;

    // These values are streamed so please don't change the integer values!
    enum EditMode
    {
        ENVELOPE = 0, // no constraints on horizontal time axis
        LFO = 1,      // MSEG editing is constrained to just one phase unit (0 ... 1)
                      // useful for single cycle waveform editing
    } editMode = ENVELOPE;

    // These values are streamed so please don't change the integer values!
    enum LoopMode
    {
        ONESHOT = 1,   // Play the MSEG front to back and then output the final value
        LOOP = 2,      // Play the MSEG front to loop end and then return to loop start
        GATED_LOOP = 3 // Play the MSEG front to loop end, then return to loop start,
                       // but if at any time a note off is generated, jump to loop end
                       // at current value and progress to end once
    } loopMode = LOOP;

    int loop_start = -1, loop_end = -1; // -1 signifies the entire MSEG in this context

    int n_activeSegments = 0;
    std::array<segment, max_msegs> segments;

    // These are calculated values which derive from the segment, which we cache for efficiency.
    // If you edit the segments, then MSEGModulationHelper::rebuildCache can rebuild them
    float totalDuration;
    std::array<float, max_msegs> segmentStart, segmentEnd;
    float durationToLoopEnd;
    float durationLoopStartToLoopEnd;
    float envelopeModeDuration = -1, envelopeModeNV1 = -2; // -2 as sentinel since NV1 is -1/1

    /*
     * These "UI" type things we decided, late in 1.8, are actually a critical part of
     * the modelling experience, so even if they aren't required to actually evaluate
     * the model, we decided to move them to the model and the patch, rather than the
     * dawExtraState. Note that vSnap and hSnap are also streamed into the DES at write
     * time and optionally streamed out based on a user preference.
     */
    static constexpr float defaultVSnapDefault = 0.25, defaultHSnapDefault = 0.125;
    float vSnapDefault = defaultVSnapDefault, hSnapDefault = defaultHSnapDefault;
    float vSnap = 0, hSnap = 0;
    float axisWidth = -1, axisStart = -1;

    static constexpr float minimumDuration = 0.0;
};

struct FormulaModulatorStorage
{
    std::string formulaString = "";
    size_t formulaHash = 0;

    // these values stream so don't change the numerical equivalents
    enum Interpreter
    {
        LUA = 1001
    } interpreter = LUA;

    void setFormula(const std::string &s)
    {
        formulaString = s;
        formulaHash = std::hash<std::string>{}(s);
    }
};

/*
** There is a collection of things we want your DAW to save about your particular instance
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
     * 2. In the SurgeGUIEditor constructor, read off the value
     * 3. In SurgeGUIEditor::populateDawExtraState write it
     * 4. In SurgeGUIEditor::loadDawExtraState read it (this will probably be pretty similar to
     *    the constructor code in step 4, but this is the step when restoring, as opposed to
     *    creating an object).
     * 5. In SurgePatch load_xml()/save_xml() read and write it
     *
     * Then the state will survive create/destroy and save/restore
     */
    struct EditorState
    {
        int instanceZoomFactor = -1;
        int current_scene = 0;
        int current_fx = 0;
        int current_osc[n_scenes] = {0};
        bool isMSEGOpen = false;
        bool msegStateIsPopulated = false;
        modsources modsource = ms_lfo1, modsource_editor[n_scenes] = {ms_lfo1, ms_lfo1};

        struct
        {
            int timeEditMode = 0;
        } msegEditState[n_scenes][n_lfos];

        /*
         * Window state parameters for Formula Editor overlay
         * codeOrPrelude: Code editor selected tab
         * debuggerOpen: Debug panel toggle
         */
        struct FormulaEditState
        {
            int codeOrPrelude{0};
            bool debuggerOpen{false};
        } formulaEditState[n_scenes][n_lfos];

        struct
        {
            bool hasCustomEditor = false;
        } oscExtraEditState[n_scenes][n_lfos];

        /*
         * Window state parameters for WTSE overlay
         * codeOrPrelude: Code editor selected tab
         */
        struct WavetableScriptEditState
        {
            int codeOrPrelude{0};
        } wavetableScriptEditState[n_scenes][n_oscs];

        struct OverlayState
        {
            int whichOverlay{-1};
            bool isTornOut{false};
            std::pair<int, int> tearOutPosition{-1, -1};
        };
        std::vector<OverlayState> activeOverlays;

        struct ModulationEditorState
        {
            int sortOrder = 0;
            int filterOn = 0;
            std::string filterString{""};
            int filterInt{0};
        } modulationEditorState;

        struct OscilloscopeOverlayState
        {
            int mode = 0; // 0 for waveform, 1 for spectrum.

            // Waveform values.
            float trigger_speed = 0.5f;
            float trigger_level = 0.5f;
            float trigger_limit = 0.5f;
            float time_window = 0.5f;
            float amp_window = 0.5f;
            int trigger_type = 0;
            bool dc_kill = false;
            bool sync_draw = false;

            // Spectrum values.
            float noise_floor = 0.f;
            float max_db = 1.f;
            float decay_rate = 1.f;
        } oscilloscopeOverlayState;

        struct TuningOverlayState
        {
            int editMode = 0;
        } tuningOverlayState;
    } editor;

    bool mpeEnabled = false;
    int mpePitchBendRange = -1;

    bool hasScale = false;
    std::string scaleContents = "";

    bool hasMapping = false;
    std::string mappingContents = "";
    std::string mappingName = "";

    bool mapChannelToOctave = false;

    std::map<int, int> midictrl_map;           // param -> midictrl
    std::map<int, int> midichan_map;           // param -> midichan
    std::map<int, int> customcontrol_map;      // custom controller number -> midictrl
    std::map<int, int> customcontrol_chan_map; // custom controller number -> midichan

    int monoPedalMode = 0;
    int oddsoundRetuneMode = 0;

    int tuningApplicationMode = 1; // RETUNE_MIDI_ONLY

    bool isDirty{false};

    bool disconnectFromOddSoundMTS{false};

    int oscPortIn{DEFAULT_OSC_PORT_IN};
    int oscPortOut{DEFAULT_OSC_PORT_OUT};
    std::string oscIPAddrOut{DEFAULT_OSC_IPADDR_OUT};
    bool oscStartIn{false};
    bool oscStartOut{false};

    fs::path lastLoadedPatch{};
};

struct PatchTuningStorage
{
    bool tuningStoredInPatch = false;
    std::string scaleContents = "";
    std::string mappingContents = "";
    std::string mappingName = "";
};

class SurgeStorage;

class SurgePatch
{
  public:
    SurgePatch(SurgeStorage *storage);
    ~SurgePatch();
    void init_default_values();
    void update_controls(bool init = false, void *init_osc = 0, bool from_stream = false);
    void do_morph();
    void copy_scenedata(pdata *, pdata *, int scene);
    void copy_globaldata(pdata *);

    // load/save
    // void load_xml();
    // void save_xml();
    void load_xml(const void *data, int size, bool preset);
    unsigned int save_xml(void **data);
    unsigned int save_RIFF(void **data);

    // Factor these so the LFO preset mechanism can use them as well
    void msegToXMLElement(MSEGStorage *ms, TiXmlElement &parent) const;
    void msegFromXMLElement(MSEGStorage *ms, TiXmlElement *parent, bool restoreSnaps) const;
    void stepSeqToXmlElement(StepSequencerStorage *ss, TiXmlElement &parent, bool streamMask) const;
    void stepSeqFromXmlElement(StepSequencerStorage *ss, TiXmlElement *parent) const;
    void formulaToXMLElement(FormulaModulatorStorage *ms, TiXmlElement &parent) const;
    void formulaFromXMLElement(FormulaModulatorStorage *ms, TiXmlElement *parent) const;

    void load_patch(const void *data, int size, bool preset);
    unsigned int save_patch(void **data);
    Parameter *parameterFromOSCName(std::string stName);

    // data
    SurgeSceneStorage scene[n_scenes], morphscene;

    FxStorage fx[n_fx_slots]{
        FxStorage(fxslot_ains1),   FxStorage(fxslot_ains2),   FxStorage(fxslot_bins1),
        FxStorage(fxslot_bins2),   FxStorage(fxslot_send1),   FxStorage(fxslot_send2),
        FxStorage(fxslot_global1), FxStorage(fxslot_global2), FxStorage(fxslot_ains3),
        FxStorage(fxslot_ains4),   FxStorage(fxslot_bins3),   FxStorage(fxslot_bins4),
        FxStorage(fxslot_send3),   FxStorage(fxslot_send4),   FxStorage(fxslot_global3),
        FxStorage(fxslot_global4)};
    int scene_start[n_scenes], scene_size;

    std::unordered_map<std::string, Parameter *> param_ptr_by_oscname;

    // streaming name for splitpoint is splitkey (due to legacy)
    Parameter scene_active, scenemode, splitpoint;
    Parameter volume;
    Parameter polylimit;
    Parameter fx_bypass, fx_disable;
    Parameter character;

    StepSequencerStorage stepsequences[n_scenes][n_lfos];
    MSEGStorage msegs[n_scenes][n_lfos];
    FormulaModulatorStorage formulamods[n_scenes][n_lfos];

    PatchTuningStorage patchTuning;
    DAWExtraStateStorage dawExtraState;

    std::vector<Parameter *> param_ptr;
    std::vector<int> easy_params_id;

    std::vector<ModulationRouting> modulation_global;
    pdata scenedata[n_scenes][n_scene_params];
    pdata scenedataOrig[n_scenes][n_scene_params];
    pdata globaldata[n_global_params];
    void *patchptr;
    SurgeStorage *storage;

    // metadata
    std::string name, category, author, license, comment;

    struct Tag
    {
        Tag() : tag("no-tag") {}
        Tag(const std::string &s) : tag(s) {}
        std::string tag;
    };
    std::vector<Tag> tags;

    std::atomic<bool> isDirty{false};

    // macro controllers
#define CUSTOM_CONTROLLER_LABEL_SIZE 20
    char CustomControllerLabel[n_customcontrollers][CUSTOM_CONTROLLER_LABEL_SIZE];

    char LFOBankLabel[n_scenes][n_lfos][max_lfo_indices][CUSTOM_CONTROLLER_LABEL_SIZE];

    int streamingRevision;
    int currentSynthStreamingRevision;

    /*
     * This parameter exists for the very special reason of maintaining compatibility with
     * comb filter tuning for streaming versions which are older than Surge v1.8.
     * Prior to that, the comb filter had a calculation error in the time and was out of tune,
     * but that lead to a unique sound in existing patches. So we introduce this parameter
     * which allows us to leave old patches mis-tuned in FilterCoefficientMaker and is handled
     * properly at stream time and so on.
     */
    bool correctlyTuneCombFilter = true;

    FilterSelectorMapper patchFilterSelectorMapper;
    WaveShaperSelectorMapper patchWaveshaperSelectorMapper;

    struct MonophonicParamModulation
    {
        int32_t param_id{0};
        double value{0};
        valtypes vt_type{vt_float};

        // Integer modulation is new and doesn't internally clamp so clamp at the modulation edge
        int imin{0}, imax{1};
    };
    int32_t paramModulationCount{0};
    static constexpr int maxMonophonicParamModulations = 256;
    std::array<MonophonicParamModulation, maxMonophonicParamModulations> monophonicParamModulations;
};

struct Patch
{
    std::string name;
    fs::path path;
    uint64_t lastModTime;
    int category;
    int order;
    bool isFavorite;
};

struct PatchCategory
{
    std::string name;
    int order;
    std::vector<PatchCategory> children;
    bool isRoot;
    bool isFactory;

    int internalid;
    int numberOfPatchesInCategory;
    int numberOfPatchesInCategoryAndChildren;
};

enum surge_copysource
{
    cp_off = 0,
    cp_scene = 1U << 1,
    cp_osc = 1U << 2,
    cp_oscmod = 1U << 3,
    cp_lfo = 1U << 4,
    cp_modulator_target = 1U << 5,
    cp_lfomod = cp_lfo | cp_modulator_target
};

class MTSClient;

/* storage layer */

namespace Surge
{

namespace FxClipboard
{
struct Clipboard;
}

namespace Storage
{
struct ScenesOutputData
{
    std::shared_ptr<float> sceneData[n_scenes][N_OUTPUTS]{{nullptr, nullptr}, {nullptr, nullptr}};

  public:
    ScenesOutputData();
    const std::shared_ptr<float> &getSceneData(int scene, int channel) const;
    void provideSceneData(int scene, int channel, float *data);
    bool thereAreClients(int scene) const;
};

struct FxUserPreset;
struct ModulatorPreset;
} // namespace Storage
namespace Memory
{
struct SurgeMemoryPools;
}
namespace Formula
{
struct GlobalData;
}
} // namespace Surge

namespace sst::basic_blocks::tables
{
struct SurgeSincTableProvider;
}

class alignas(16) SurgeStorage
{
  public:
    float audio_in alignas(16)[2][BLOCK_SIZE_OS];
    float audio_in_nonOS alignas(16)[2][BLOCK_SIZE];
    // this will be a pointer to an aligned 2 x BLOCK_SIZE_OS array
    float audio_otherscene alignas(16)[2][BLOCK_SIZE_OS];

    std::unique_ptr<sst::basic_blocks::tables::SurgeSincTableProvider> sincTableProvider;
    float *sinctable, *sinctable1X;
    int16_t *sinctableI16;

    float table_dB alignas(16)[512], table_envrate_lpf alignas(16)[512],
        table_envrate_linear alignas(16)[512], table_glide_exp alignas(16)[512],
        table_glide_log alignas(16)[512];
    float samplerate{0}, samplerate_inv{1};
    double dsamplerate{0}, dsamplerate_inv{1};
    double dsamplerate_os{0}, dsamplerate_os_inv{1};
    fs::path lastLoadedPatch{};
    // Ring buffer that holds the audio output, used for the oscilloscope. Will hold a bit under 1/4
    // second of data, assuming the sample rate is 48k.
    sst::cpputils::StereoRingBuffer<float, 8192> audioOut;

    struct SurgeStorageConfig
    {
        std::string suppliedDataPath{""};
        bool createUserDirectory{true};
        fs::path extraThirdPartyWavetablesPath{};
        fs::path extraUsersWavetablesPath{};
        bool scanWavetableAndPatches{true};

        static SurgeStorageConfig fromDataPath(const std::string &s)
        {
            auto r = SurgeStorageConfig();
            r.suppliedDataPath = s;
            return r;
        }
    };
    SurgeStorage(const SurgeStorageConfig &);
    SurgeStorage(std::string suppliedDataPath = "")
        : SurgeStorage(SurgeStorageConfig::fromDataPath(suppliedDataPath))
    {
    }

    static std::string skipPatchLoadDataPathSentinel;

    // In Surge XT, SurgeStorage can now keep a cache of errors it reports to the user
    enum ErrorType
    {
        GENERAL_ERROR = 1,
        AUDIO_INPUT_LATENCY_WARNING = 2,
    };
    void reportError(const std::string &msg, const std::string &title,
                     const ErrorType errorType = GENERAL_ERROR, bool reportToStdout = true);
    struct ErrorListener
    {
        // This can be called from any thread, beware! But it is called only
        // when an error occursm so if you want to be sloppy and just lock, that's OK
        virtual void onSurgeError(const std::string &msg, const std::string &title,
                                  const ErrorType &errorType) = 0;
    };
    std::unordered_set<ErrorListener *> errorListeners;
    // this mutex is ONLY locked in the error path and when registering a listener
    // (from the UI thread)
    std::mutex preListenerErrorMutex;
    std::vector<std::tuple<std::string, std::string, ErrorType>> preListenerErrors;
    void addErrorListener(ErrorListener *l)
    {
        errorListeners.insert(l);
        std::lock_guard<std::mutex> g(preListenerErrorMutex);
        for (auto p : preListenerErrors)
            l->onSurgeError(std::get<0>(p), std::get<1>(p), std::get<2>(p));
        preListenerErrors.clear();
    }
    void removeErrorListener(ErrorListener *l) { errorListeners.erase(l); }

    enum OkCancel
    {
        OK,
        CANCEL
    };

    std::function<void(const std::string &msg, const std::string &title, OkCancel def,
                       std::function<void(OkCancel)>)>
        okCancelProvider = [](const std::string &, const std::string &, OkCancel def,
                              std::function<void(OkCancel)> callback) { callback(def); };
    void clearOkCancelProvider()
    {
        okCancelProvider = [](const std::string &, const std::string &, OkCancel def,
                              std::function<void(OkCancel)> callback) { callback(def); };
    }

    std::string initPatchName{"Init Saw"}, initPatchCategory{"Templates"},
        initPatchCategoryType{"Factory"};

    static constexpr int tuning_table_size = 512;
    float table_pitch alignas(16)[tuning_table_size];
    float table_pitch_inv alignas(16)[tuning_table_size];
    float table_note_omega alignas(16)[2][tuning_table_size];
    float table_pitch_ignoring_tuning alignas(16)[tuning_table_size];
    float table_pitch_inv_ignoring_tuning alignas(16)[tuning_table_size];
    float table_note_omega_ignoring_tuning alignas(16)[2][tuning_table_size];
    // 2^0 -> 2^+/-1/12th. See comment in note_to_pitch
    float table_two_to_the alignas(16)[1001];
    float table_two_to_the_minus alignas(16)[1001];

    ~SurgeStorage();

    std::unique_ptr<Surge::PatchStorage::PatchDB> patchDB;
    bool patchDBInitialized{false};
    void initializePatchDb(bool forcePatchRescan = false);

    std::unique_ptr<Surge::Storage::UserDefaultsProvider> userDefaultsProvider;

    std::unique_ptr<SurgePatch> _patch;
    std::unique_ptr<Surge::Formula::GlobalData> formulaGlobalData;

    // SurgePatch &getPatch();
    SurgePatch &getPatch() const;

    float pitch_bend;

    float vu_falloff;
    float temposyncratio, temposyncratio_inv; // 1.f is 120 BPM
    float unstreamedTempo{120.f};             // this one is in actual BPM
    double songpos;
    void init_tables();
    float nyquist_pitch;
    int last_key[2]; // TODO: FIX SCENE ASSUMPTION
    TiXmlElement *getSnapshotSection(const char *name);
    void load_midi_controllers();
    void write_midi_controllers_to_user_default();
    void save_snapshots();
    int controllers[n_customcontrollers];
    int controllers_chan[n_customcontrollers];
    float poly_aftertouch[2][16][128]; // TODO: FIX SCENE ASSUMPTION
    float modsource_vu[n_modsources];
    void setSamplerate(float sr);
    float cpu_falloff;

    bool oscReceiving{false};
    bool oscSending{false};

    /*
     * A bit of a cheat - this isn't really the storage, but is updated by the synth at render time.
     * We put it here so LFOS can get it, which formula needs
     */
    std::atomic<int> activeVoiceCount{0};

    bool getOverrideDataHome(std::string &value);
    void createUserDirectory();

    void refresh_wtlist();
    void refresh_wtlistAddDir(bool userDir, const std::string &subdir);
    void refresh_wtlistFrom(bool isUser, const fs::path &from, const std::string &subdir);
    void refresh_patchlist();
    void refreshPatchlistAddDir(bool userDir, std::string subdir);

    void refreshPatchOrWTListAddDir(bool userDir, const fs::path &fromPath, std::string subdir,
                                    std::function<bool(std::string)> filterOp,
                                    std::vector<Patch> &items,
                                    std::vector<PatchCategory> &categories);

    void perform_queued_wtloads();

    void load_wt(int id, Wavetable *wt, OscillatorStorage *);
    void load_wt(std::string filename, Wavetable *wt, OscillatorStorage *);
    bool load_wt_wt(std::string filename, Wavetable *wt, std::string &metadata);
    bool load_wt_wt_mem(const char *data, const size_t dataSize, Wavetable *wt);
    bool load_wt_wav_portable(std::string filename, Wavetable *wt, std::string &metadata);
    std::string export_wt_wav_portable(const std::string &fbase, Wavetable *wt,
                                       const std::string &metadata);
    std::string export_wt_wav_portable(const fs::path &fpath, Wavetable *wt,
                                       const std::string &metadata);
    bool export_wt_wt_portable(const fs::path &fpath, Wavetable *wt, const std::string &metadata);

    std::string make_wt_metadata(OscillatorStorage *);
    bool parse_wt_metadata(const std::string &, OscillatorStorage *);

    void clipboard_copy(int type, int scene, int entry, modsources ms = ms_original);
    // this function is a bit of a hack to stop me having a reference to SurgeSynth here
    // and also to stop me having to move all of isValidModulation and its buddies onto SurgeStorage
    void clipboard_paste(
        int type, int scene, int entry, modsources ms = ms_original,
        std::function<bool(int, modsources)> isValidModulation = [](auto a,
                                                                    auto b) { return true; },
        std::function<void(std::unique_ptr<Surge::FxClipboard::Clipboard> &, int)> updateFxInSlot =
            [](auto &a, auto b) {});
    ;
    int get_clipboard_type() const;
    // direction: false for previous, true for next
    int getAdjacentWaveTable(int id, bool direction) const;
    std::string getCurrentWavetableName(OscillatorStorage *);

    // The in-memory patch database
    std::vector<Patch> patch_list;
    std::vector<PatchCategory> patch_category;
    int firstThirdPartyCategory;
    int firstUserCategory;
    std::vector<int> patchOrdering;
    std::vector<int> patchCategoryOrdering;
    std::array<std::array<int, 128>, 128> patchIdToMidiBankAndProgram;

    // The in-memory wavetable database
    std::vector<Patch> wt_list;
    std::vector<PatchCategory> wt_category;
    int firstThirdPartyWTCategory;
    int firstUserWTCategory;
    std::vector<int> wtOrdering;
    std::vector<int> wtCategoryOrdering;

    std::unique_ptr<Surge::Storage::FxUserPreset> fxUserPreset;
    std::unique_ptr<Surge::Storage::ModulatorPreset> modulatorPreset;

    bool datapathOverriden{false};
    fs::path datapath;
    fs::path userDefaultFilePath;
    fs::path userDataPath;
    fs::path userPatchesPath;
    fs::path userPatchesMidiProgramChangePath;
    fs::path userWavetablesPath;
    fs::path userModulatorSettingsPath;
    fs::path userFXPath;
    fs::path userWavetablesExportPath;
    fs::path userSkinsPath;
    fs::path userMidiMappingsPath;
    fs::path extraThirdPartyWavetablesPath; // used by rack
    fs::path extraUserWavetablesPath;       // used by rack

    std::atomic<bool> userDataPathValid{false};

    std::string midiProgramChangePatchesSubdir{"MIDI Programs"};

    std::map<std::string, TiXmlDocument> userMidiMappingsXMLByName;
    void rescanUserMidiMappings();
    void loadMidiMappingByName(std::string name);
    void storeMidiMappingToName(std::string name);

    std::mutex waveTableDataMutex;
    std::recursive_mutex modRoutingMutex;
    Wavetable WindowWT;

    // hardclip
    enum HardClipMode
    {
        HARDCLIP_TO_18DBFS = 1,
        HARDCLIP_TO_0DBFS,
        BYPASS_HARDCLIP // scene only
    } hardclipMode = HARDCLIP_TO_18DBFS,
      sceneHardclipMode[n_scenes] = {HARDCLIP_TO_18DBFS, HARDCLIP_TO_18DBFS};

    void loadTuningFromSCL(const fs::path &p);
    void loadMappingFromKBM(const fs::path &p);
    std::function<void()> onTuningChanged{nullptr};

    float note_to_pitch(float x);
    float note_to_pitch_inv(float x);
    float note_to_pitch_ignoring_tuning(float x);
    float note_to_pitch_inv_ignoring_tuning(float x);
    inline float note_to_pitch_tuningctr(float x)
    {
        return note_to_pitch(x + scaleConstantNote()) * scaleConstantPitchInv();
    }
    inline float note_to_pitch_inv_tuningctr(float x)
    {
        return note_to_pitch_inv(x + scaleConstantNote()) * scaleConstantPitch();
    }

    void note_to_omega(float, float &, float &);
    void note_to_omega_ignoring_tuning(float, float &, float &, float sampleRate = 0.0f);

    /*
     * Tuning Support and Tuning State. Here's how it works
     *
     * We have SCL, KBM and Tunings and use the nomelcature "Scale", "Mapping", and "Tuning"
     * consistently as of 1.9. The Tuning is the combination of a scale and mapping which results
     * in pitches. We now consistently manage that three-part state in the functions below and
     * make them available here as read-only state. (I mean I guess you could write directly
     * to these members, but don't).
     *
     * The functions we provide are
     * - retuneTo12TetCStandardMapping  (basically resets everything)
     * - retuneTo12TETScale (resets scale, leaves mapping intact)
     * - reemapToConcertCKeyboard (leaves scale, resets mapping)
     * - retuneToScale( s ) (resets scale, leaves mapping)
     * - remapToKeyboard( k ) (leaves scale, resets mapping)
     * - retuneTosCaleAndMapping() ( pretty obvious right?)
     */
    Tunings::Tuning twelveToneStandardMapping;
    Tunings::Tuning currentTuning;
    Tunings::Scale currentScale;
    Tunings::KeyboardMapping currentMapping;
    bool isStandardTuning = true, isStandardScale = true, isStandardMapping = true;

    Tunings::Tuning patchStoredTuning;
    bool hasPatchStoredTuning{false};

    std::atomic<bool> mapChannelToOctave; // When other midi modes come along, clean this up.

    bool mpeEnabled{false};

    int oscPortIn{DEFAULT_OSC_PORT_IN};
    int oscPortOut{DEFAULT_OSC_PORT_OUT};
    std::string oscOutIP{"127.0.0.1"};
    bool oscStartIn{false};
    bool oscStartOut{false};
    bool echoMIDIctrlToOSC{true}; // This may be made UI- or OSC-switchable in future

    static constexpr double MIDI_0_FREQ = Tunings::MIDI_0_FREQ;
    // this value needs to be passed along to FilterCoefficientMaker

    enum TuningApplicationMode
    {
        RETUNE_ALL = 0, // These values are streamed so don't change them if you add
        RETUNE_MIDI_ONLY = 1
    } tuningApplicationMode = RETUNE_MIDI_ONLY,
      patchStoredTuningApplicationMode =
          tuningApplicationMode; // This is the default as of 1.9/sv16

    float tuningPitch = 32.0f, tuningPitchInv = 0.03125f;

    Tunings::Scale cachedToggleOffScale;
    Tunings::KeyboardMapping cachedToggleOffMapping;
    bool isToggledToCache;
    bool togglePriorState[3];
    void toggleTuningToCache();
    bool isStandardTuningAndHasNoToggle();
    void resetTuningToggle();

    std::atomic<uint64_t> tuningUpdates{2}; // the 'last sent' starts at 0. just a different value
    bool retuneToScale(const Tunings::Scale &s)
    {
        currentScale = s;
        isStandardTuning = false;
        isStandardScale = false;
        return resetToCurrentScaleAndMapping();
    }
    bool retuneTo12TETScale()
    {
        currentScale = Tunings::evenTemperament12NoteScale();
        isStandardScale = true;
        isStandardTuning = isStandardMapping;

        return resetToCurrentScaleAndMapping();
    }
    bool retuneTo12TETScaleC261Mapping()
    {
        currentScale = Tunings::evenTemperament12NoteScale();
        currentMapping = Tunings::KeyboardMapping();
        isStandardTuning = true;
        isStandardScale = true;
        isStandardMapping = true;
        resetToCurrentScaleAndMapping();
        init_tables();
        return true;
    }
    bool remapToKeyboard(const Tunings::KeyboardMapping &k)
    {
        currentMapping = k;
        isStandardMapping = false;
        isStandardTuning = false;

        tuningPitch = k.tuningFrequency / Tunings::MIDI_0_FREQ;
        tuningPitchInv = 1.0 / tuningPitch;
        return resetToCurrentScaleAndMapping();
    }

    bool remapToConcertCKeyboard()
    {
        auto k = Tunings::KeyboardMapping();
        currentMapping = k;
        isStandardMapping = true;
        isStandardTuning = isStandardScale;

        tuningPitch = 32.0;
        tuningPitchInv = 1.0 / 32.0;
        return resetToCurrentScaleAndMapping();
    }

    bool retuneAndRemapToScaleAndMapping(const Tunings::Scale &s, const Tunings::KeyboardMapping &k)
    {
        currentMapping = k;
        currentScale = s;
        isStandardMapping = false;
        isStandardScale = false;
        isStandardTuning = false;

        return resetToCurrentScaleAndMapping();
    }

    // Critically this does not touch the "isStandard" variables at all
    bool resetToCurrentScaleAndMapping();

    inline int scaleConstantNote()
    {
        if (tuningApplicationMode == RETUNE_ALL)
        {
            return currentMapping.tuningConstantNote;
        }
        else
        {
            // In this case the mapping happens at the keyboard layer so don't double-retune it
            return 60;
        }
    }
    inline float scaleConstantPitch() { return tuningPitch; }
    inline float scaleConstantPitchInv()
    {
        return tuningPitchInv;
    } // Obviously that's the inverse of the above

    float remapKeyInMidiOnlyMode(float inKey);

    void setTuningApplicationMode(const TuningApplicationMode m);
    TuningApplicationMode getTuningApplicationMode() const;

#ifndef SURGE_SKIP_ODDSOUND_MTS
    void initialize_oddsound();
    void deinitialize_oddsound();
    void setOddsoundMTSActiveTo(bool b);

    void connect_as_oddsound_main();
    void disconnect_as_oddsound_main();
    uint64_t lastSentTuningUpdate{0}; // since tuning update starts at 2
    void send_tuning_update();
    std::atomic<bool> uiThreadChecksTunings{false};
#endif
    MTSClient *oddsound_mts_client = nullptr;
    std::atomic<bool> oddsound_mts_active_as_client{false};
    uint32_t oddsound_mts_on_check = 0;
    std::atomic<bool> oddsound_mts_active_as_main{false};
    enum OddsoundRetuneMode
    {
        RETUNE_CONSTANT = 0,
        RETUNE_NOTE_ON_ONLY = 1
    } oddsoundRetuneMode = RETUNE_CONSTANT;

    /*
     * If we tune at keyboard or with MTS, we don't reset the internal tuning table.
     * Same if we are in standard tuning. So let's have that condition be done once
     */
    inline bool tuningTableIs12TET()
    {
        if ((isStandardTuning) ||                                     // nothing changed
            (oddsound_mts_client && oddsound_mts_active_as_client) || // MTS in command
            tuningApplicationMode == RETUNE_MIDI_ONLY // tune the keyboard, not the tables
        )
            return true;
        return false;
    }

    std::string guessAtKBMName(const Tunings::KeyboardMapping &k)
    {
        auto res = std::string("");
        res += "Root " + std::to_string(k.middleNote);
        res += " with note " + std::to_string(k.tuningConstantNote);
        res += " @ " + std::to_string(k.tuningFrequency) + "Hz";
        return res;
    }

    Modulator::SmoothingMode smoothingMode = Modulator::SmoothingMode::LEGACY;
    Modulator::SmoothingMode pitchSmoothingMode = Modulator::SmoothingMode::LEGACY;
    float mpePitchBendRange = -1.0f;

    std::atomic<int> otherscene_clients;

    Surge::Storage::ScenesOutputData scenesOutputData;

    std::unordered_map<int, std::string> helpURL_controlgroup;
    std::unordered_map<std::string, std::string> helpURL_paramidentifier;
    std::unordered_map<std::string, std::string> helpURL_specials;
    // Alternatively, make this unordered and provide a hash
    std::map<std::pair<std::string, int>, std::string> helpURL_paramidentifier_typespecialized;

    int subtypeMemory[n_scenes][n_filterunits_per_scene][sst::filters::num_filter_types];
    MonoPedalMode monoPedalMode = HOLD_ALL_NOTES;

  private:
    TiXmlDocument snapshotloader;
    std::vector<Parameter> clipboard_p;
    int clipboard_type;
    StepSequencerStorage clipboard_stepsequences[n_lfos];
    // UP this for the forward decl fix
    std::unique_ptr<Surge::FxClipboard::Clipboard> clipboard_scenefx[n_fx_per_chain];
    MSEGStorage clipboard_msegs[n_lfos];
    FormulaModulatorStorage clipboard_formulae[n_lfos];
    OscillatorStorage::ExtraConfigurationData clipboard_extraconfig[n_oscs];
    std::vector<ModulationRouting> clipboard_modulation_scene, clipboard_modulation_voice,
        clipboard_modulation_global;
    Wavetable clipboard_wt[n_oscs];
    std::array<std::string, n_oscs> clipboard_wt_names;
    char clipboard_modulator_names[n_lfos][max_lfo_indices][CUSTOM_CONTROLLER_LABEL_SIZE + 1];
    MonoVoicePriorityMode clipboard_primode = NOTE_ON_LATEST_RETRIGGER_HIGHEST;
    MonoVoiceEnvelopeMode clipboard_envmode = RESTART_FROM_ZERO;

  public:
    // whether to skip loading, desired while exporting manifests. Only used by LV2 currently.
    static bool skipLoadWtAndPatch;

    std::unique_ptr<Surge::Memory::SurgeMemoryPools> memoryPools;

/*
 * An RNG which is decoupled from the non-Surge global state and is threadsafe.
 * This RNG has the semantic that it is seeded when the first Surge in your session
 * uses it on a given thread, and then retains its independent state. It is designed
 * to have the same API as std::rand, so 'std::rand -> storage::rand' is a good change.
 *
 * Reseeding it impacts the global state on that thread.
 */
#define STORAGE_USES_INDEPENDENT_RNG 1
#if STORAGE_USES_INDEPENDENT_RNG
    /*
     * Turn this back on with a different threading check
     */
    struct RNGGen
    {
        RNGGen()
            : g(std::chrono::system_clock::now().time_since_epoch().count()), d(0, RAND_MAX),
              pm1(-1.f, 1.f), z1(0.f, 1.f), u32(0, 0xFFFFFFFF)
        {
        }
        std::minstd_rand g;
        std::uniform_int_distribution<int> d;
        std::uniform_real_distribution<float> pm1, z1;
        std::uniform_int_distribution<uint32_t> u32;
    } rngGen;

#define DEBUG_RNG_THREADING 0
#if DEBUG_RNG_THREADING
    std::thread::id audioThreadID{0};
    inline void runningOnAudioThread()
    {
        if (audioThreadID && std::this_thread::get_id() != audioThreadID)
        {
            std::cout << "BUM CALL ON NON AUDIO THREAD" << std::endl;
        }
    }
#else
#define runningOnAudioThread() (void *)0;
#endif
    /*
     * These API points are only thread safe on the AUDIO thread.
     * If you want to have an independent RNG on another thread, manage
     * your lifecycle yourself or if you want make a new instance of the
     * Storage::RNGGen utility class above
     */
    inline int rand()
    {
        runningOnAudioThread();
        return rngGen.d(rngGen.g);
    }
    inline uint32_t rand_u32()
    {
        runningOnAudioThread();
        return rngGen.u32(rngGen.g);
    }
    inline float rand_pm1()
    {
        runningOnAudioThread();
        return rngGen.pm1(rngGen.g);
    }
    inline float rand_01()
    {
        runningOnAudioThread();
        return rngGen.z1(rngGen.g);
    }
// void seed_rand(int s) { rngGen.g.seed(s); }
#else
    inline int rand() { return std::rand(); }
    inline uint32_t rand_u32() { return (uint32_t)(rand_01() * (float)(0xFFFFFFFF)); }
    inline float rand_pm1() { return rand_01() * 2 - 1; }
    inline float rand_01() { return (float)std::rand() / (float)(RAND_MAX); }
#endif
    float db_to_linear(float);
    float lookup_waveshape(sst::waveshapers::WaveshaperType, float);
    float lookup_waveshape_warp(sst::waveshapers::WaveshaperType, float);
    float envelope_rate_lpf(float);
    float envelope_rate_linear(float);
    float envelope_rate_linear_nowrap(float);
    float glide_log(float);
    float glide_exp(float);
};

namespace Surge
{
namespace Storage
{
bool isValidName(const std::string &name);
// is this really not in stdlib?
bool isValidUTF8(const std::string &testThis);

std::string findReplaceSubstring(std::string &source, const std::string &from,
                                 const std::string &to);

bool is_base64(unsigned char c);
std::string base64_encode(unsigned char const *bytes_to_encode, unsigned int in_len);
std::string base64_decode(std::string const &encoded_string);

bool getValueDispPrecision(SurgeStorage *storage);

} // namespace Storage
} // namespace Surge

/*
** ToElement does this && check to check nulls (as does ToDocument and so on).
** gcc -O3 on Linux optimizes that away giving crashes. So do this instead
** See GitHub issue #469
*/
#define TINYXML_SAFE_TO_ELEMENT(expr) ((expr) ? (expr)->ToElement() : NULL)

#endif // SURGE_SRC_COMMON_SURGESTORAGE_H
