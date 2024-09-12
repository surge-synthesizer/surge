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

#ifndef SURGE_SRC_COMMON_DSP_SURGEVOICE_H
#define SURGE_SRC_COMMON_DSP_SURGEVOICE_H
#include "SurgeStorage.h"
#include "Oscillator.h"
#include "SurgeVoiceState.h"
#include "ADSRModulationSource.h"
#include "LFOModulationSource.h"
#include <vembertech/lipol.h>
#include "QuadFilterChain.h"
#include <array>

struct QuadFilterChainState;

class alignas(16) SurgeVoice
{
  public:
    float output alignas(16)[2][BLOCK_SIZE_OS];
    lipol_ps osclevels alignas(16)[7];
    pdata localcopy alignas(16)[n_scene_params];
    pdata localcopy2 alignas(16)[n_scene_params];
    float fmbuffer alignas(16)[BLOCK_SIZE_OS];

    // used for the 2>1<3 FM-mode (Needs the pointer earlier)

    SurgeVoice();
    SurgeVoice(SurgeStorage *storage, SurgeSceneStorage *scene, pdata *params, pdata *paramsUnmod,
               int key, int velocity, int channel, int scene_id, float detune,
               MidiKeyState *keyState, MidiChannelState *mainChannelState,
               MidiChannelState *voiceChannelState, bool mpeEnabled, int64_t voiceOrder,
               int32_t host_note_id, int16_t originating_host_key, int16_t originating_host_channel,
               float aegStart, float fegStart);
    ~SurgeVoice();

    void release();
    void uber_release();

    void sampleRateReset();
    bool process_block(QuadFilterChainState &, int);
    void GetQFB(); // Get the updated registers from the QuadFB
    void legato(int key, int velocity, char detune);
    void switch_toggled();
    void freeAllocatedElements();
    int osctype[n_oscs];
    SurgeVoiceState state;
    int age, age_release;

    bool matchesChannelKeyId(int16_t channel, int16_t key, int32_t host_noteid);

    /*
     * Begin implementing host-provided identifiers for voices for polyphonic
     * modulators, note expressions, and so on
     */
    int32_t host_note_id{-1};
    int16_t originating_host_key{-1}, originating_host_channel{-1};

    struct PolyphonicParamModulation
    {
        int32_t param_id{0};
        double value{0};
        // see the comments in the monophonic implementation
        valtypes vt_type{vt_float};
        int imin{0}, imax{1};
    };
    int32_t paramModulationCount{0};
    static constexpr int maxPolyphonicParamModulations = 64;
    std::array<PolyphonicParamModulation, maxPolyphonicParamModulations> polyphonicParamModulations;
    // See comment in SurgeSynthesizer::applyParameterPolyphonicModulation for why this has 2 args
    void applyPolyphonicParamModulation(Parameter *, double value, double underlyingMonoMod);

    enum NoteExpressionType
    {
        VOLUME,   // maps to per voice volume in all voices
                  // 0 < x < = 4, amp = 20 * log(x)
        PAN,      // maps to per voice pan in all voices, 0..1 with 0.5 center
        PITCH,    // maps to the tuning -120 to 120 in keys
        TIMBRE,   // maps to the MPE Timbre parameter 0 .. 1
        PRESSURE, // maps to "channel AT" in MPE mode and "poly AT" in non-MPE mode 0 .. 1
        UNKNOWN
    };
    void applyNoteExpression(NoteExpressionType net, float value);
    static constexpr int numNoteExpressionTypes = (int)UNKNOWN;
    std::array<float, numNoteExpressionTypes> noteExpressions;

    /*
    ** Given a note0 and an oscillator this returns the appropriate note.
    ** This is a pretty easy calculation in non-absolute mode. Just add.
    ** But in absolute mode you need to find the virtual note which would
    ** map to that frequency shift.
    */
    inline float noteShiftFromPitchParam(float note0 /* the key + octave */,
                                         int oscNum /* The osc for pitch diffs */)
    {
        if (localcopy[scene->osc[oscNum].pitch.param_id_in_scene].f == 0)
        {
            return note0;
        }

        if (scene->osc[oscNum].pitch.absolute)
        {
            // remember note_to_pitch is linear interpolation on storage->table_pitch from
            // position note + 256 % 512
            // OK so now what we are searching for is the pair which surrounds us plus the pitch
            // drift... so
            float fqShift = 10 * localcopy[scene->osc[oscNum].pitch.param_id_in_scene].f *
                            (scene->osc[oscNum].pitch.extend_range ? 12.f : 1.f);
            float tableNote0 = note0 + 256;

            int tableIdx = (int)tableNote0;
            if (tableIdx > 0x1fe)
                tableIdx = 0x1fe;
            float tableFrac = tableNote0 - tableIdx;

            // so just iterate up. Deal with negative also of course. Since we will always be close
            // just do it brute force for now but later we can do a binary or some such.
            float pitch0 = storage->table_pitch[tableIdx] * (1.0 - tableFrac) +
                           storage->table_pitch[tableIdx + 1] * tableFrac;
            float targetPitch = pitch0 + fqShift / Tunings::MIDI_0_FREQ;
            if (targetPitch < 0)
                targetPitch = 0.01;

            if (fqShift > 0)
            {
                while (tableIdx < 0x1fe)
                {
                    float pitch1 = storage->table_pitch[tableIdx + 1];
                    if (pitch0 <= targetPitch && pitch1 > targetPitch)
                    {
                        break;
                    }
                    pitch0 = pitch1;
                    tableIdx++;
                }
            }
            else
            {
                while (tableIdx > 0)
                {
                    float pitch1 = storage->table_pitch[tableIdx - 1];
                    if (pitch0 >= targetPitch && pitch1 < targetPitch)
                    {
                        tableIdx--;
                        break;
                    }
                    pitch0 = pitch1;
                    tableIdx--;
                }
            }

            // So what's the frac
            // (1-x) * [tableIdx] + x * [tableIdx+1] = targetPitch
            // Or: x = ( target - table) / ( [ table+1 ] - [table] );
            float frac = (targetPitch - storage->table_pitch[tableIdx]) /
                         (storage->table_pitch[tableIdx + 1] - storage->table_pitch[tableIdx]);
            // frac = 1 -> targetpitch = +1; frac = 0 -> targetPitch

            // std::cout << note0 << " " << tableIdx << " " << frac << " " << fqShift << " " <<
            // targetPitch << std::endl;
            return tableIdx + frac - 256;
        }
        else
        {
            return note0 + localcopy[scene->osc[oscNum].pitch.param_id_in_scene].f *
                               (scene->osc[oscNum].pitch.extend_range ? 12.f : 1.f);
        }
    }

    void getAEGFEGLevel(float &aeg, float &feg)
    {
        aeg = ampEGSource.get_output(0);
        feg = filterEGSource.get_output(0);
    }

    void restartAEGFEGAttack(float aeg, float feg)
    {
        ampEGSource.attackFrom(aeg);
        filterEGSource.attackFrom(feg);
    }

    void resetVelocity(int midiVelocity)
    {
        state.velocity = midiVelocity;
        state.fvel = midiVelocity / 127.0;
        velocitySource.set_target(0, state.fvel);
    }

    void retriggerLFOEnvelopes();
    void retriggerOSCWithIndependentAttacks();
    void resetPortamentoFrom(int key, int channel);

    static float channelKeyEquvialent(float key, int channel, bool isMpeEnabled,
                                      SurgeStorage *storage, bool remapKeyForTuning = true);

  private:
    template <bool first> void calc_ctrldata(QuadFilterChainState *, int);

    /*
     * Some modulations at the voice level were applied to the local
     * copy used by the voice LFO before the attack began. This meant,
     * basically, that modulating LFO amplitude with velocity didn't quite
     * work.
     *
     * So change the structure of SurgeVoice to have an applyModulationToLocalcopy
     * method which either can or cannot skip LFO sources (since it can't apply
     * the local sources pre-attack) and then call that before the attck in
     * voice initiation, and again after attack to finish the matrix (via
     * calc_ctrldata)
     */
    template <bool noLFOSources = false> void applyModulationToLocalcopy();

    void update_portamento();
    void set_path(bool osc1, bool osc2, bool osc3, int FMmode, bool ring12, bool ring23,
                  bool noise);
    int routefilter(int);
    void retriggerPortaIfKeyChanged();

    LFOModulationSource lfo[n_lfos_voice];

    // Filterblock state storage
    void SetQFB(QuadFilterChainState *, int); // Set the parameters & registers
    QuadFilterChainState *fbq;
    int fbqi;

    struct
    {
        float Gain, FB, Mix1, Mix2, OutL, OutR, Out2L, Out2R, Drive, wsLPF, FBlineL, FBlineR;
        float Delay[4][sst::filters::utilities::MAX_FB_COMB +
                       sst::filters::utilities::SincTable::FIRipol_N];
        struct
        {
            float C[sst::filters::n_cm_coeffs], R[sst::filters::n_filter_registers];
            unsigned int WP;
            int type, subtype; // used for comparison with the last run
        } FU[4];
        struct
        {
            float R[sst::waveshapers::n_waveshaper_registers];
        } WS[2];
    } FBP;
    sst::filters::FilterCoefficientMaker<SurgeStorage> CM[2];

    // data
    int lag_id[8], pitch_id, octave_id, volume_id, pan_id, width_id;
    SurgeSceneStorage *scene;
    pdata *paramptr;
    pdata *paramptrUnmod;
    int route[6];

    float octaveSize = 12.0f;

    bool osc1, osc2, osc3, ring12, ring23, noise;
    int FMmode;
    float noisegenL[2], noisegenR[2];

    Oscillator *osc[n_oscs];
    unsigned char oscbuffer alignas(16)[n_oscs][oscillator_buffer_size];

  public: // this is public, but only for the regtests
    std::array<ModulationSource *, n_modsources> modsources;
    SurgeStorage *storage;

  public:
    ControllerModulationSource velocitySource;
    ModulationSource releaseVelocitySource;
    ModulationSource keytrackSource;
    ControllerModulationSource polyAftertouchSource;
    ControllerModulationSource monoAftertouchSource;
    ControllerModulationSource timbreSource;
    ModulationSource rndUni, rndBi, altUni, altBi;
    ADSRModulationSource ampEGSource;
    ADSRModulationSource filterEGSource;

    // filterblock stuff
    int id_cfa, id_cfb, id_kta, id_ktb, id_emoda, id_emodb, id_resoa, id_resob, id_drive, id_vca,
        id_vcavel, id_fbalance, id_feedback;

    // MPE special cases
    bool mpeEnabled;
};

void all_ring_modes_block(float *__restrict src1_l, float *__restrict src2_l,
                          float *__restrict src1_r, float *__restrict src2_r,
                          float *__restrict dst_l, float *__restrict dst_r, bool is_wide, int mode,
                          lipol_ps osclevels, unsigned int nquads);

#endif // SURGE_SRC_COMMON_DSP_SURGEVOICE_H
