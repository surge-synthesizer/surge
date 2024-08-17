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

#include "SurgeVoice.h"
#include "UserDefaults.h"
#include "DSPUtils.h"
#include "QuadFilterChain.h"
#include "globals.h"
#include <cmath>
#ifndef SURGE_SKIP_ODDSOUND_MTS
#include "libMTSClient.h"
#endif

#include "sst/basic-blocks/mechanics/block-ops.h"
#include "sst/basic-blocks/dsp/Clippers.h"
#include "sst/basic-blocks/dsp/CorrelatedNoise.h"
#include "CXOR.h"

using namespace std;
namespace mech = sst::basic_blocks::mechanics;
namespace sdsp = sst::basic_blocks::dsp;

enum lag_entries
{
    le_osc1,
    le_osc2,
    le_osc3,
    le_noise,
    le_ring12,
    le_ring23,
    le_pfg,
};

inline void set1f(__m128 &m, int i, float f) { *((float *)&m + i) = f; }

inline void set1i(__m128 &m, int e, int i) { *((int *)&m + e) = i; }
inline void set1ui(__m128 &m, int e, unsigned int i) { *((unsigned int *)&m + e) = i; }

inline float get1f(__m128 m, int i) { return *((float *)&m + i); }

float SurgeVoiceState::getPitch(SurgeStorage *storage)
{
    float mpeBend = mpePitchBend.get_output(0) * mpePitchBendRange;
    /*
    ** For this commented out section, see the comment on MPE global pitch bend in
    *SurgeSynthesizer::pitchBend
    */
    auto res = key + /* mainChannelState->pitchBendInSemitones + */ mpeBend + detune;

#ifndef SURGE_SKIP_ODDSOUND_MTS
    if (storage->oddsound_mts_client && storage->oddsound_mts_active_as_client)
    {
        if (storage->oddsoundRetuneMode == SurgeStorage::RETUNE_CONSTANT ||
            key != keyRetuningForKey)
        {
            keyRetuningForKey = key;
            keyRetuning = MTS_RetuningInSemitones(storage->oddsound_mts_client, key + mpeBend,
                                                  mtsUseChannelWhenRetuning ? 0 : channel);
        }
        auto rkey = keyRetuning;

        res = res + rkey;
    }
    else
#endif
        if (!storage->isStandardTuning &&
            storage->tuningApplicationMode == SurgeStorage::RETUNE_MIDI_ONLY)
    {
        res = storage->remapKeyInMidiOnlyMode(res);
    }

    res = SurgeVoice::channelKeyEquvialent(res, channel, mpeEnabled, storage, false);

    return res;
}

float SurgeVoice::channelKeyEquvialent(float key, int channel, bool isMpeEnabled,
                                       SurgeStorage *storage, bool remapKeyForTuning)
{
    float res = key;
    if (storage->mapChannelToOctave && !isMpeEnabled)
    {
        if (remapKeyForTuning)
        {
            res = storage->remapKeyInMidiOnlyMode(res);
        }

        float shift;
        if (channel > 7)
        {
            shift = channel - 16;
        }
        else
        {
            shift = channel;
        }
        if (storage->tuningApplicationMode == SurgeStorage::RETUNE_ALL)
        {
            // keys are in scale space so move scale.count
            res += storage->currentScale.count * shift;
        }
        else
        {
            // keys are in tuning space so move cents worth of keys
            // We don't know the period in MTS-ESP so default to octave for now
            if (storage->isStandardTuning || storage->oddsound_mts_active_as_client)
                res += 12 * shift;
            else
            {
                auto ct = storage->currentScale.tones[storage->currentScale.count - 1].cents;
                res += ct / 100 * shift;
            }
        }
    }
    return res;
}

// This is super useful for debugging placement new issues. Please leave it here until
// we stabilize 1.3.1 at least
// #define VOICE_LIFETIME_DEBUG 1
#ifdef VOICE_LIFETIME_DEBUG
int voiceCDCount{0};
#endif

SurgeVoice::SurgeVoice()
{
#ifdef VOICE_LIFETIME_DEBUG
    voiceCDCount++;
    std::cout << "Calling SurgeVoice CTOR NoArg " << voiceCDCount << std::endl;
#endif
}

SurgeVoice::SurgeVoice(SurgeStorage *storage, SurgeSceneStorage *oscene, pdata *params, int key,
                       int velocity, int channel, int scene_id, float detune,
                       MidiKeyState *keyState, MidiChannelState *mainChannelState,
                       MidiChannelState *voiceChannelState, bool mpeEnabled, int64_t voiceOrder,
                       int32_t host_nid, int16_t host_key, int16_t host_chan, float aegStart,
                       float fegStart)
//: fb(storage,oscene)
{
#ifdef VOICE_LIFETIME_DEBUG
    voiceCDCount++;
    std::cout << "Calling SurgeVoice CTOR FullArg " << voiceCDCount << std::endl;
#endif
    // assign pointers
    this->storage = storage;
    this->scene = oscene;
    this->paramptr = params;
    this->mpeEnabled = mpeEnabled;
    this->host_note_id = host_nid;
    this->originating_host_key = host_key;
    this->originating_host_channel = host_chan;
    this->paramModulationCount = 0;
    assert(storage);
    assert(oscene);

    sampleRateReset();
    memcpy(localcopy, paramptr, sizeof(localcopy));

    noteExpressions[VOLUME] = 1.0; // 1 is no amplification
    noteExpressions[PAN] = 0.5;    // since it is 0 ... 1
    noteExpressions[PITCH] = 0.0;
    noteExpressions[TIMBRE] = 0.0;
    noteExpressions[PRESSURE] = 0.0;

    // We want this on the keystate so it survives the voice for mono mode
    keyState->voiceOrder = voiceOrder;

    state.voiceOrderAtCreate = voiceOrder;

    age = 0;
    age_release = 0;

    state.key = key;
    state.keyRetuningForKey = -1000;
    state.channel = channel;

    state.velocity = velocity;
    state.fvel = velocity / 127.f;

    state.releasevelocity = 0;
    state.freleasevel = 0;

    state.scene_id = scene_id;
    state.detune = detune;
    state.uberrelease = false;
    state.keyState = keyState;
    state.mainChannelState = mainChannelState;
    state.voiceChannelState = voiceChannelState;

    state.mpePitchBendRange = storage->mpePitchBendRange;
    state.mpeEnabled = mpeEnabled;
    state.mpePitchBend = ControllerModulationSource(storage->pitchSmoothingMode);
    state.mpePitchBend.set_samplerate(storage->samplerate, storage->samplerate_inv);
    state.mpePitchBend.init(voiceChannelState->pitchBend / 8192.f);

#ifndef SURGE_SKIP_ODDSOUND_MTS
    const bool isCh23Mode = Surge::Storage::getUserDefaultValue(
        storage, Surge::Storage::UseCh2Ch3ToPlayScenesIndividually, true, false);
    const bool isChSplitMode = storage->getPatch().scenemode.val.i == sm_chsplit;

    state.mtsUseChannelWhenRetuning =
        (mpeEnabled || isChSplitMode || isCh23Mode || storage->mapChannelToOctave);
#endif

    resetPortamentoFrom(storage->last_key[scene_id], channel);

    storage->last_key[scene_id] = key;
    noisegenL[0] = 0.f;
    noisegenR[0] = 0.f;
    noisegenL[1] = 0.f;
    noisegenR[1] = 0.f;

    // set states & variables
    state.gate = true;
    state.keep_playing = true;

    // init subcomponents
    for (int i = 0; i < n_oscs; i++)
    {
        osctype[i] = -1;
    }

    memset(&FBP, 0, sizeof(FBP));
    sampleRateReset();

    polyAftertouchSource = ControllerModulationSource(storage->smoothingMode);
    polyAftertouchSource.set_samplerate(storage->samplerate, storage->samplerate_inv);

    monoAftertouchSource = ControllerModulationSource(storage->smoothingMode);
    monoAftertouchSource.set_samplerate(storage->samplerate, storage->samplerate_inv);

    timbreSource = ControllerModulationSource(storage->smoothingMode);
    timbreSource.set_samplerate(storage->samplerate, storage->samplerate_inv);

    polyAftertouchSource.init(
        storage->poly_aftertouch[state.scene_id & 1][state.channel & 15][state.key & 127]);
    timbreSource.init(state.voiceChannelState->timbre);
    monoAftertouchSource.init(state.voiceChannelState->pressure);

    lag_id[le_osc1] = scene->level_o1.param_id_in_scene;
    lag_id[le_osc2] = scene->level_o2.param_id_in_scene;
    lag_id[le_osc3] = scene->level_o3.param_id_in_scene;
    lag_id[le_noise] = scene->level_noise.param_id_in_scene;
    lag_id[le_ring12] = scene->level_ring_12.param_id_in_scene;
    lag_id[le_ring23] = scene->level_ring_23.param_id_in_scene;
    lag_id[le_pfg] = scene->level_pfg.param_id_in_scene;

    volume_id = scene->volume.param_id_in_scene;
    pan_id = scene->pan.param_id_in_scene;
    width_id = scene->width.param_id_in_scene;
    pitch_id = scene->pitch.param_id_in_scene;
    octave_id = scene->octave.param_id_in_scene;

    for (int i = 0; i < n_lfos_voice; i++)
    {
        lfo[i].assign(storage, &scene->lfo[i], localcopy, &state,
                      &storage->getPatch().stepsequences[state.scene_id][i],
                      &storage->getPatch().msegs[state.scene_id][i],
                      &storage->getPatch().formulamods[state.scene_id][i]);
        lfo[i].setIsVoice(true);

        if (scene->lfo[i].shape.val.i == lt_formula)
        {
            Surge::Formula::setupEvaluatorStateFrom(lfo[i].formulastate, storage->getPatch(),
                                                    scene_id);
            Surge::Formula::setupEvaluatorStateFrom(lfo[i].formulastate, this);
        }

        modsources[ms_lfo1 + i] = &lfo[i];
    }

    modsources[ms_velocity] = &velocitySource;
    modsources[ms_releasevelocity] = &releaseVelocitySource;
    modsources[ms_keytrack] = &keytrackSource;
    modsources[ms_polyaftertouch] = &polyAftertouchSource;

    // Velocity is *almost* never reset except in poly mode,
    // so init it here then make it adapt smoothly.
    velocitySource.init(0, state.fvel);
    velocitySource.smoothingMode = Modulator::SmoothingMode::SLOW_EXP;
    velocitySource.set_samplerate(storage->samplerate, storage->samplerate_inv);
    releaseVelocitySource.set_output(0, state.freleasevel);
    keytrackSource.set_output(0, 0.f);

    ampEGSource.init(storage, &scene->adsr[0], localcopy, &state);
    filterEGSource.init(storage, &scene->adsr[1], localcopy, &state);

    modsources[ms_ampeg] = &ampEGSource;
    modsources[ms_filtereg] = &filterEGSource;
    modsources[ms_modwheel] = oscene->modsources[ms_modwheel];
    modsources[ms_breath] = oscene->modsources[ms_breath];
    modsources[ms_expression] = oscene->modsources[ms_expression];
    modsources[ms_sustain] = oscene->modsources[ms_sustain];
    modsources[ms_aftertouch] = &monoAftertouchSource;
    modsources[ms_lowest_key] = oscene->modsources[ms_lowest_key];
    modsources[ms_highest_key] = oscene->modsources[ms_highest_key];
    modsources[ms_latest_key] = oscene->modsources[ms_latest_key];
    modsources[ms_timbre] = &timbreSource;
    modsources[ms_pitchbend] = oscene->modsources[ms_pitchbend];
    modsources[ms_slfo1] = oscene->modsources[ms_slfo1];
    modsources[ms_slfo2] = oscene->modsources[ms_slfo2];
    modsources[ms_slfo3] = oscene->modsources[ms_slfo3];
    modsources[ms_slfo4] = oscene->modsources[ms_slfo4];
    modsources[ms_slfo5] = oscene->modsources[ms_slfo5];
    modsources[ms_slfo6] = oscene->modsources[ms_slfo6];

    for (int i = 0; i < n_customcontrollers; i++)
    {
        modsources[ms_ctrl1 + i] = oscene->modsources[ms_ctrl1 + i];
    }

    /*
    ** We want to snap the rnd and alt
    */
    int ao = oscene->modsources[ms_random_unipolar]->get_active_outputs();
    rndUni.set_active_outputs(ao);
    for (int w = 0; w < ao; ++w)
        rndUni.set_output(w, oscene->modsources[ms_random_unipolar]->get_output(w));
    modsources[ms_random_unipolar] = &rndUni;

    ao = oscene->modsources[ms_random_bipolar]->get_active_outputs();
    rndBi.set_active_outputs(ao);
    for (int w = 0; w < ao; ++w)
        rndBi.set_output(w, oscene->modsources[ms_random_bipolar]->get_output(w));
    modsources[ms_random_bipolar] = &rndBi;

    altUni.set_output(0, oscene->modsources[ms_alternate_unipolar]->get_output(0));
    modsources[ms_alternate_unipolar] = &altUni;

    altBi.set_output(0, oscene->modsources[ms_alternate_bipolar]->get_output(0));
    modsources[ms_alternate_bipolar] = &altBi;

    id_cfa = scene->filterunit[0].cutoff.param_id_in_scene;
    id_cfb = scene->filterunit[1].cutoff.param_id_in_scene;
    id_kta = scene->filterunit[0].keytrack.param_id_in_scene;
    id_ktb = scene->filterunit[1].keytrack.param_id_in_scene;
    id_emoda = scene->filterunit[0].envmod.param_id_in_scene;
    id_emodb = scene->filterunit[1].envmod.param_id_in_scene;
    id_resoa = scene->filterunit[0].resonance.param_id_in_scene;
    id_resob = scene->filterunit[1].resonance.param_id_in_scene;
    id_drive = scene->wsunit.drive.param_id_in_scene;
    id_vca = scene->vca_level.param_id_in_scene;
    id_vcavel = scene->vca_velsense.param_id_in_scene;
    id_fbalance = scene->filter_balance.param_id_in_scene;
    id_feedback = scene->feedback.param_id_in_scene;

    applyModulationToLocalcopy<true>();

    ampEGSource.attackFrom(aegStart);
    filterEGSource.attackFrom(fegStart);

    for (int i = 0; i < n_lfos_voice; i++)
    {
        lfo[i].attack();
    }

    calc_ctrldata<true>(0, 0); // init interpolators
    SetQFB(0, 0);              // init Quad Filter Block parameter interpolators

    // It is important that switch_toggled comes last since it creates and activates the
    // oscillators which need the modulation state set in calc_ctrldata to get sample 0
    // right.
    switch_toggled();
}

SurgeVoice::~SurgeVoice()
{
#ifdef VOICE_LIFETIME_DEBUG
    voiceCDCount--;
    std::cout << "Calling SurgeVoice DTOR " << voiceCDCount << std::endl;
#endif
}

void SurgeVoice::legato(int key, int velocity, char detune)
{
    if (state.portaphase > 1)
        state.portasrc_key = state.getPitch(storage);
    else
    {
        // exponential or linear key traversal for the portamento
        float phase;
        switch (scene->portamento.porta_curve)
        {
        case porta_log:
            phase = storage->glide_log(state.portaphase);
            break;
        case porta_lin:
            phase = state.portaphase;
            break;
        case porta_exp:
            phase = storage->glide_exp(state.portaphase);
            break;
        }

        state.portasrc_key = ((1 - phase) * state.portasrc_key + phase * state.getPitch(storage));

        if (scene->portamento.porta_gliss) // quantize portamento to keys
            state.pkey = floor(state.pkey + 0.5);

        state.porta_doretrigger = false;
        if (scene->portamento.porta_retrigger)
            retriggerPortaIfKeyChanged();
    }

    state.key = key;
    storage->last_key[state.scene_id] = key;
    state.portaphase = 0;

    /*state.velocity = velocity;
    state.fvel = velocity/127.f;*/
}

void SurgeVoice::retriggerPortaIfKeyChanged()
{
    if (!storage->oddsound_mts_active_as_client &&
        (storage->isStandardTuning || storage->tuningApplicationMode == SurgeStorage::RETUNE_ALL))
    {
        if (floor(state.pkey + 0.5) != state.priorpkey)
        {
            state.priorpkey = floor(state.pkey + 0.5);
            state.porta_doretrigger = true;
        }
    }
    else
    {
        std::function<float(int)> v4k = [this](int k) {
            return storage->currentTuning.logScaledFrequencyForMidiNote(k) * 12;
        };

#ifndef SURGE_SKIP_ODDSOUND_MTS
        if (storage->oddsound_mts_client && storage->oddsound_mts_active_as_client)
        {
            v4k = [this](int k) {
                return log2f(MTS_NoteToFrequency(storage->oddsound_mts_client, k, state.channel) /
                             Tunings::MIDI_0_FREQ) *
                       12;
            };
        }
#endif

        auto ckey = state.pkey;
        auto start = state.priorpkey - 2;
        auto prior = v4k(start);
        while (prior > ckey && start > -250)
        {
            start -= 10;
            prior = v4k(start);
        }
        for (int i = start; i < 256; ++i)
        {
            auto next = v4k(i);
            if (next > state.pkey && prior <= state.pkey)
            {
                auto frac = (state.pkey - prior) / (next - prior);
                ckey = i + frac;
                break;
            }
            prior = next;
        }

        if (floor(ckey + 0.5) != state.priorpkey)
        {
            state.priorpkey = floor(ckey + 0.5);
            state.porta_doretrigger = true;
        }
    }
}

void SurgeVoice::switch_toggled()
{
    update_portamento();
    float pb = modsources[ms_pitchbend]->get_output(0);
    if (pb > 0)
        pb *= (float)scene->pbrange_up.val.i * (scene->pbrange_up.extend_range ? 0.01f : 1.f);
    else
        pb *= (float)scene->pbrange_dn.val.i * (scene->pbrange_dn.extend_range ? 0.01f : 1.f);

    // scenepbpitch is pitch state but without state.pkey, so that it can be used to add
    // scene pitch/octave, pitch bend and associated modulations to non-keytracked oscillators
    state.scenepbpitch = pb;
    state.pitch = state.pkey + state.scenepbpitch;

    modsources[ms_keytrack]->set_output(0, (state.pitch - (float)scene->keytrack_root.val.i) *
                                               (1.0f / 12.0f));

    /*
     * Since we have updated the keytrack output here we need to re-update the localcopy modulators
     */
    vector<ModulationRouting>::iterator iter;
    iter = scene->modulation_voice.begin();
    while (iter != scene->modulation_voice.end())
    {
        int src_id = iter->source_id;
        int dst_id = iter->destination_id;
        float depth = iter->depth;
        if (modsources[src_id] && src_id == ms_keytrack)
        {
            localcopy[dst_id].f +=
                depth * modsources[ms_keytrack]->get_output(0) * (1 - iter->muted);
        }
        iter++;
    }

    for (int i = 0; i < n_oscs; i++)
    {
        if (osctype[i] != scene->osc[i].type.val.i)
        {
            bool nzid = scene->drift.extend_range;
            osc[i] = spawn_osc(scene->osc[i].type.val.i, storage, &scene->osc[i], localcopy,
                               oscbuffer[i]);
            if (osc[i])
            {
                // this matches the override in ::process_block
                float ktrkroot = 60;
                auto usep = noteShiftFromPitchParam(
                    (scene->osc[i].keytrack.val.b ? state.pitch : ktrkroot + state.scenepbpitch) +
                        octaveSize * scene->osc[i].octave.val.i,
                    0);
                osc[i]->init(usep, false, nzid);
            }
            osctype[i] = scene->osc[i].type.val.i;
        }
    }

    int FM = scene->fm_switch.val.i;
    switch (FM)
    {
    case fm_off:
        break;
    case fm_2to1:
        if (osc[1] && osc[0])
            osc[0]->assign_fm(osc[1]->output);
        break;
    case fm_3to2to1:
        if (osc[1] && osc[0])
            osc[0]->assign_fm(osc[1]->output);
        if (osc[2] && osc[1])
            osc[1]->assign_fm(osc[2]->output);
        break;
    case fm_2and3to1:
        if (osc[0])
            osc[0]->assign_fm(fmbuffer);
        break;
    }

    bool solo = (scene->solo_o1.val.b || scene->solo_o2.val.b || scene->solo_o3.val.b ||
                 scene->solo_noise.val.b || scene->solo_ring_12.val.b || scene->solo_ring_23.val.b);
    if (solo)
    {
        set_path(scene->solo_o1.val.b, scene->solo_o2.val.b, scene->solo_o3.val.b,
                 FM && scene->solo_o1.val.b,
                 // inter-osc FM should be enabled only if carrier (osc
                 // 1) is soloed, in case any solos are active
                 scene->solo_ring_12.val.b, scene->solo_ring_23.val.b, scene->solo_noise.val.b);

        // pre-1.7.2 unique (single) solo paths:
        /*
        if (scene->solo_o1.val.b)
           set_path(true, false, false, false, false, false, false);
        else if (scene->solo_o2.val.b)
           set_path(false, true, false, FM, false, false, false);
        else if (scene->solo_o3.val.b)
           set_path(false, false, true, false, false, false, false);
        else if (scene->solo_noise.val.b)
           set_path(false, false, false, false, false, false, true);
        else if (scene->solo_ring_12.val.b)
           set_path(false, false, false, false, true, false, false);
        else if (scene->solo_ring_23.val.b)
           set_path(false, false, false, false, false, true, false);
        */
    }
    else
    {
        bool use_osc1 = (!scene->mute_o1.val.b);
        bool use_osc2 = (!scene->mute_o2.val.b);
        bool use_osc3 = (!scene->mute_o3.val.b);
        bool use_ring12 = (!scene->mute_ring_12.val.b);
        bool use_ring23 = (!scene->mute_ring_23.val.b);
        bool use_noise = (!scene->mute_noise.val.b);

        set_path(use_osc1, use_osc2, use_osc3, FM, use_ring12, use_ring23, use_noise);
    }

    // check the filtertype
    for (int u = 0; u < n_filterunits_per_scene; u++)
    {
        if ((scene->filterunit[u].type.val.i != FBP.FU[u].type) ||
            (scene->filterunit[u].subtype.val.i != FBP.FU[u].subtype))
        {
            memset(&FBP.FU[u], 0, sizeof(FBP.FU[u]));
            FBP.FU[u].type = scene->filterunit[u].type.val.i;
            FBP.FU[u].subtype = scene->filterunit[u].subtype.val.i;

            if (scene->filterblock_configuration.val.i == fc_wide)
            {
                memset(&FBP.FU[u + 2], 0, sizeof(FBP.FU[u + 2]));
                FBP.FU[u + 2].type = scene->filterunit[u].type.val.i;
                FBP.FU[u + 2].subtype = scene->filterunit[u].subtype.val.i;
            }

            CM[u].Reset();
        }
    }
}

void SurgeVoice::release()
{
    ampEGSource.release();
    filterEGSource.release();

    for (int i = 0; i < n_lfos_voice; i++)
    {
        lfo[i].release();
    }

    state.gate = false;
    releaseVelocitySource.set_output(0, state.releasevelocity / 127.0f);
}

void SurgeVoice::uber_release()
{
    ampEGSource.uber_release();
    state.gate = false;
    state.uberrelease = true;
}

void SurgeVoice::update_portamento()
{
    float const_rate_factor = 1.0;
    int quantStep = 12;

    if (!storage->isStandardTuning && storage->currentScale.count > 1)
    {
        quantStep = storage->currentScale.count;
    }

    // portamento constant rate mode (multiply portamento time with every octave traversed
    // (or scale length in case of microtuning)
    if (scene->portamento.porta_constrate)
    {
        const_rate_factor =
            (1.f /
             ((1.f / quantStep) * fabs(state.getPitch(storage) - state.portasrc_key) + 0.00001));
    }

    float portaClamp = 4;
    // more than 16 second portamento with the modulation is painful. See #7301
    assert(scene->portamento.val_max.f < portaClamp);
    state.portaphase +=
        storage->envelope_rate_linear(
            std::min(localcopy[scene->portamento.param_id_in_scene].f, portaClamp)) *
        (scene->portamento.temposync ? storage->temposyncratio : 1.f) * const_rate_factor;

    if ((state.portaphase < 1) &&
        (localcopy[scene->portamento.param_id_in_scene].f > scene->portamento.val_min.f))
    {
        // exponential or linear key traversal for the portamento
        float phase;
        switch (scene->portamento.porta_curve)
        {
        case porta_log:
            phase = storage->glide_log(state.portaphase);
            break;
        case porta_lin:
            phase = state.portaphase;
            break;
        case porta_exp:
            phase = storage->glide_exp(state.portaphase);
            break;
        }

        state.pkey = (1.f - phase) * state.portasrc_key + (float)phase * state.getPitch(storage);

        // quantize portamento to keys
        if (scene->portamento.porta_gliss)
        {
            state.pkey = floor(state.pkey + 0.5);
        }

        state.porta_doretrigger = false;

        if (scene->portamento.porta_retrigger)
        {
            retriggerPortaIfKeyChanged();
        }
    }
    else
    {
        state.pkey = state.getPitch(storage);
    }

    state.pkey += noteExpressions[PITCH];
}

int SurgeVoice::routefilter(int r)
{
    switch (scene->filterblock_configuration.val.i)
    {
    case fc_serial1:
    case fc_serial2:
    case fc_serial3:
        if (r == 1)
            r = 0;
        break;
    }
    return r;
}

template <bool first> void SurgeVoice::calc_ctrldata(QuadFilterChainState *Q, int e)
{
    // Always process LFO1 so the gate retrigger always work
    lfo[0].process_block();
    velocitySource.process_block();

    for (int i = 0; i < n_lfos_voice; i++)
    {
        if (scene->lfo[i].shape.val.i == lt_formula)
        {
            Surge::Formula::setupEvaluatorStateFrom(lfo[i].formulastate, storage->getPatch(),
                                                    state.scene_id);
            Surge::Formula::setupEvaluatorStateFrom(lfo[i].formulastate, this);
        }

        if (i != 0 && scene->modsource_doprocess[ms_lfo1 + i])
        {
            lfo[i].process_block();
        }
    }

    auto pm = scene->polymode.val.i;

    bool fromCurrent = (pm == pm_poly && scene->polyVoiceRepeatedKeyMode ==
                                             PolyVoiceRepeatedKeyMode::ONE_VOICE_PER_KEY) ||
                       (pm != pm_poly &&
                        scene->monoVoiceEnvelopeMode == MonoVoiceEnvelopeMode::RESTART_FROM_LATEST);

    for (int i = 0; i < n_lfos_voice; ++i)
    {
        if (lfo[i].retrigger_AEG)
        {
            auto ms = ((ADSRModulationSource *)modsources[ms_ampeg]);
            auto val = fromCurrent * ms->get_output(0);

            ms->retriggerFrom(val);
        }
        if (lfo[i].retrigger_FEG)
        {
            auto ms = ((ADSRModulationSource *)modsources[ms_filtereg]);
            auto val = fromCurrent * ms->get_output(0);

            ms->retriggerFrom(val);
        }
    }

    modsources[ms_ampeg]->process_block();
    modsources[ms_filtereg]->process_block();

    if (((ADSRModulationSource *)modsources[ms_ampeg])->is_idle())
    {
        state.keep_playing = false;
    }

    // TODO: memcpy is bottleneck
    // don't actually need to copy everything
    // LFOs could be ignored when unused
    // same for FX & OSCs
    // also ignore integer parameters
    memcpy(localcopy, paramptr, sizeof(localcopy));

    applyModulationToLocalcopy();
    update_portamento();

    if (state.porta_doretrigger)
    {
        state.porta_doretrigger = false;
        ((ADSRModulationSource *)modsources[ms_ampeg])->retrigger();
        ((ADSRModulationSource *)modsources[ms_filtereg])->retrigger();
    }

    float pb = modsources[ms_pitchbend]->get_output(0);

    if (pb > 0)
        pb *= (float)scene->pbrange_up.val.i * (scene->pbrange_up.extend_range ? 0.01f : 1.f);
    else
        pb *= (float)scene->pbrange_dn.val.i * (scene->pbrange_dn.extend_range ? 0.01f : 1.f);

    octaveSize = 12.0f;
    if (!storage->isStandardTuning && storage->tuningApplicationMode == SurgeStorage::RETUNE_ALL)
        octaveSize = storage->currentScale.count;

    state.scenepbpitch = pb + localcopy[pitch_id].f * (scene->pitch.extend_range ? 12.f : 1.f) +
                         (octaveSize * localcopy[octave_id].i);
    state.pitch = state.pkey + state.scenepbpitch;

    // I didn't change this for octaveSize, I think rightly
    modsources[ms_keytrack]->set_output(0, (state.pitch - (float)scene->keytrack_root.val.i) *
                                               (1.0f / 12.0f));

    if (scene->modsource_doprocess[ms_polyaftertouch])
    {
        auto addl = 0.0;
        if (!mpeEnabled)
            addl = noteExpressions[PRESSURE];
        ((ControllerModulationSource *)modsources[ms_polyaftertouch])
            ->set_target(
                addl +
                storage->poly_aftertouch[state.scene_id & 1][state.channel & 15][state.key & 127]);
        ((ControllerModulationSource *)modsources[ms_polyaftertouch])->process_block();
    }

    float o1 = amp_to_linear(localcopy[lag_id[le_osc1]].f);
    float o2 = amp_to_linear(localcopy[lag_id[le_osc2]].f);
    float o3 = amp_to_linear(localcopy[lag_id[le_osc3]].f);
    float on = amp_to_linear(localcopy[lag_id[le_noise]].f);
    float r12 = amp_to_linear(localcopy[lag_id[le_ring12]].f);
    float r23 = amp_to_linear(localcopy[lag_id[le_ring23]].f);
    float pfg = storage->db_to_linear(localcopy[lag_id[le_pfg]].f);

    osclevels[le_osc1].set_target(o1);
    osclevels[le_osc2].set_target(o2);
    osclevels[le_osc3].set_target(o3);
    osclevels[le_noise].set_target(on);
    osclevels[le_ring12].set_target(r12);
    osclevels[le_ring23].set_target(r23);
    osclevels[le_pfg].set_target(pfg);

    route[0] = routefilter(scene->route_o1.val.i);
    route[1] = routefilter(scene->route_o2.val.i);
    route[2] = routefilter(scene->route_o3.val.i);
    route[3] = routefilter(scene->route_ring_12.val.i);
    route[4] = routefilter(scene->route_ring_23.val.i);
    route[5] = routefilter(scene->route_noise.val.i);

    float pan1 = limit_range(localcopy[pan_id].f + state.voiceChannelState->pan +
                                 state.mainChannelState->pan + (noteExpressions[PAN] * 2 - 1),
                             -1.f, 1.f);
    float amp =
        0.5f * amp_to_linear(localcopy[volume_id].f); // the *0.5 multiplication will be eliminated
                                                      // by the 2x gain of the halfband filter

    amp = amp * noteExpressions[VOLUME]; // since amp is already linear as is the NE

    // Volume correcting/correction (fc_stereo updated since v1.2.2)
    if (scene->filterblock_configuration.val.i == fc_wide)
        amp *= 0.6666666f;
    else if (scene->filterblock_configuration.val.i == fc_stereo)
        amp *= 1.3333333f;

    if ((scene->filterblock_configuration.val.i == fc_stereo) ||
        (scene->filterblock_configuration.val.i == fc_wide))
    {
        // I am surprised this is not pan1 - as oppsoed to pan_id - so will change it
        // pan1 -= localcopy[width_id].f;
        // float pan2 = localcopy[pan_id].f + localcopy[width_id].f;
        float pan2 = pan1 + localcopy[width_id].f;
        pan1 -= localcopy[width_id].f;

        float amp2L = amp * megapanL(pan2);
        float amp2R = amp * megapanR(pan2);
        if (Q)
        {
            set1f(Q->Out2L, e, FBP.Out2L);
            set1f(Q->dOut2L, e, (amp2L - FBP.Out2L) * BLOCK_SIZE_OS_INV);
            set1f(Q->Out2R, e, FBP.Out2R);
            set1f(Q->dOut2R, e, (amp2R - FBP.Out2R) * BLOCK_SIZE_OS_INV);
        }
        FBP.Out2L = amp2L;
        FBP.Out2R = amp2R;
    }

    float ampL = amp * megapanL(pan1);
    float ampR = amp * megapanR(pan1);

    if (Q)
    {
        set1f(Q->OutL, e, FBP.OutL);
        set1f(Q->dOutL, e, (ampL - FBP.OutL) * BLOCK_SIZE_OS_INV);
        set1f(Q->OutR, e, FBP.OutR);
        set1f(Q->dOutR, e, (ampR - FBP.OutR) * BLOCK_SIZE_OS_INV);
    }

    FBP.OutL = ampL;
    FBP.OutR = ampR;
}

void SurgeVoice::sampleRateReset()
{
    for (auto &cm : CM)
        cm.setSampleRateAndBlockSize((float)storage->dsamplerate_os, BLOCK_SIZE_OS);
}

inline void all_ring_modes_block(float *__restrict src1_l, float *__restrict src2_l,
                                 float *__restrict src1_r, float *__restrict src2_r,
                                 float *__restrict dst_l, float *__restrict dst_r, bool is_wide,
                                 int mode, lipol_ps osclevels, unsigned int nquads)
{
    if (is_wide)
    {
        switch (mode)
        {
        case CombinatorMode::cxm_ring:
            mech::mul_block<BLOCK_SIZE_OS>(src1_l, src2_l, dst_l);
            mech::mul_block<BLOCK_SIZE_OS>(src1_r, src2_r, dst_r);
            break;
        case CombinatorMode::cxm_cxor43_0:
            cxor43_0_block(src1_l, src2_l, dst_l, nquads);
            cxor43_0_block(src1_r, src2_r, dst_r, nquads);
            break;
        case CombinatorMode::cxm_cxor43_1:
            cxor43_1_block(src1_l, src2_l, dst_l, nquads);
            cxor43_1_block(src1_r, src2_r, dst_r, nquads);
            break;
        case CombinatorMode::cxm_cxor43_2:
            cxor43_2_block(src1_l, src2_l, dst_l, nquads);
            cxor43_2_block(src1_r, src2_r, dst_r, nquads);
            break;
        case CombinatorMode::cxm_cxor43_3_legacy:
            cxor43_3_legacy_block(src1_l, src2_l, dst_l, nquads);
            cxor43_3_legacy_block(src1_r, src2_r, dst_r, nquads);
            break;
        case CombinatorMode::cxm_cxor43_4_legacy:
            cxor43_4_legacy_block(src1_l, src2_l, dst_l, nquads);
            cxor43_4_legacy_block(src1_r, src2_r, dst_r, nquads);
            break;
        case CombinatorMode::cxm_cxor43_3:
            cxor43_3_block(src1_l, src2_l, dst_l, nquads);
            cxor43_3_block(src1_r, src2_r, dst_r, nquads);
            break;
        case CombinatorMode::cxm_cxor43_4:
            cxor43_4_block(src1_l, src2_l, dst_l, nquads);
            cxor43_4_block(src1_r, src2_r, dst_r, nquads);
            break;
        case CombinatorMode::cxm_cxor93_0:
            cxor93_0_block(src1_l, src2_l, dst_l, nquads);
            cxor93_0_block(src1_r, src2_r, dst_r, nquads);
            break;
        case CombinatorMode::cxm_cxor93_1:
            cxor93_1_block(src1_l, src2_l, dst_l, nquads);
            cxor93_1_block(src1_r, src2_r, dst_r, nquads);
            break;
        case CombinatorMode::cxm_cxor93_2:
            cxor93_2_block(src1_l, src2_l, dst_l, nquads);
            cxor93_2_block(src1_r, src2_r, dst_r, nquads);
            break;
        case CombinatorMode::cxm_cxor93_3:
            cxor93_3_block(src1_l, src2_l, dst_l, nquads);
            cxor93_3_block(src1_r, src2_r, dst_r, nquads);
            break;
        case CombinatorMode::cxm_cxor93_4:
            cxor93_4_block(src1_l, src2_l, dst_l, nquads);
            cxor93_4_block(src1_r, src2_r, dst_r, nquads);
            break;
        }
        osclevels.multiply_2_blocks(dst_l, dst_r, nquads);
    }
    else
    {
        switch (mode)
        {
        case CombinatorMode::cxm_ring:
            mech::mul_block<BLOCK_SIZE_OS>(src1_l, src2_l, dst_l);
            break;
        case CombinatorMode::cxm_cxor43_0:
            cxor43_0_block(src1_l, src2_l, dst_l, nquads);
            break;
        case CombinatorMode::cxm_cxor43_1:
            cxor43_1_block(src1_l, src2_l, dst_l, nquads);
            break;
        case CombinatorMode::cxm_cxor43_2:
            cxor43_2_block(src1_l, src2_l, dst_l, nquads);
            break;
        case CombinatorMode::cxm_cxor43_3_legacy:
            cxor43_3_legacy_block(src1_l, src2_l, dst_l, nquads);
            break;
        case CombinatorMode::cxm_cxor43_4_legacy:
            cxor43_4_legacy_block(src1_l, src2_l, dst_l, nquads);
            break;
        case CombinatorMode::cxm_cxor43_3:
            cxor43_3_block(src1_l, src2_l, dst_l, nquads);
            break;
        case CombinatorMode::cxm_cxor43_4:
            cxor43_4_block(src1_l, src2_l, dst_l, nquads);
            break;
        case CombinatorMode::cxm_cxor93_0:
            cxor93_0_block(src1_l, src2_l, dst_l, nquads);
            break;
        case CombinatorMode::cxm_cxor93_1:
            cxor93_1_block(src1_l, src2_l, dst_l, nquads);
            break;
        case CombinatorMode::cxm_cxor93_2:
            cxor93_2_block(src1_l, src2_l, dst_l, nquads);
            break;
        case CombinatorMode::cxm_cxor93_3:
            cxor93_3_block(src1_l, src2_l, dst_l, nquads);
            break;
        case CombinatorMode::cxm_cxor93_4:
            cxor93_4_block(src1_l, src2_l, dst_l, nquads);
            break;
        }
        osclevels.multiply_block(dst_l, nquads);
    }
}

bool SurgeVoice::process_block(QuadFilterChainState &Q, int Qe)
{
    calc_ctrldata<0>(&Q, Qe);

    bool is_wide = scene->filterblock_configuration.val.i == fc_wide;
    float tblock alignas(16)[BLOCK_SIZE_OS], tblock2 alignas(16)[BLOCK_SIZE_OS];
    float *tblockR = is_wide ? tblock2 : tblock;

    // float ktrkroot = (float)scene->keytrack_root.val.i;
    // this mysterious override is duplicated in the ->init calls
    float ktrkroot = 60;
    float drift = localcopy[scene->drift.param_id_in_scene].f;

    // clear output
    mech::clear_block<BLOCK_SIZE_OS>(output[0]);
    mech::clear_block<BLOCK_SIZE_OS>(output[1]);

    for (int i = 0; i < n_oscs; ++i)
    {
        if (osc[i])
        {
            osc[i]->setGate(state.gate);
        }
    }

    if (osc3 || ring23 || ((osc1 || osc2 || ring12) && (FMmode == fm_3to2to1)) ||
        ((osc1 || ring12) && (FMmode == fm_2and3to1)))
    {
        osc[2]->process_block(
            noteShiftFromPitchParam(
                (scene->osc[2].keytrack.val.b ? state.pitch : ktrkroot + state.scenepbpitch) +
                    octaveSize * scene->osc[2].octave.val.i,
                2),
            drift, is_wide);

        if (osc3)
        {
            if (is_wide)
            {
                osclevels[le_osc3].multiply_2_blocks_to(osc[2]->output, osc[2]->outputR, tblock,
                                                        tblockR, BLOCK_SIZE_OS_QUAD);
            }
            else
            {
                osclevels[le_osc3].multiply_block_to(osc[2]->output, tblock, BLOCK_SIZE_OS_QUAD);
            }

            if (route[2] < 2)
            {
                mech::accumulate_from_to<BLOCK_SIZE_OS>(tblock, output[0]);
            }
            if (route[2] > 0)
            {
                mech::accumulate_from_to<BLOCK_SIZE_OS>(tblockR, output[1]);
            }
        }
    }

    if (osc2 || ring12 || ring23 || (FMmode && osc1))
    {
        if (FMmode == fm_3to2to1)
        {
            osc[1]->process_block(
                noteShiftFromPitchParam(
                    (scene->osc[1].keytrack.val.b ? state.pitch : ktrkroot + state.scenepbpitch) +
                        octaveSize * scene->osc[1].octave.val.i,
                    1),
                drift, is_wide, true,
                storage->db_to_linear(localcopy[scene->fm_depth.param_id_in_scene].f));
        }
        else
        {
            osc[1]->process_block(
                noteShiftFromPitchParam(
                    (scene->osc[1].keytrack.val.b ? state.pitch : ktrkroot + state.scenepbpitch) +
                        octaveSize * scene->osc[1].octave.val.i,
                    1),
                drift, is_wide);
        }

        if (osc2)
        {
            if (is_wide)
            {
                osclevels[le_osc2].multiply_2_blocks_to(osc[1]->output, osc[1]->outputR, tblock,
                                                        tblockR, BLOCK_SIZE_OS_QUAD);
            }
            else
            {
                osclevels[le_osc2].multiply_block_to(osc[1]->output, tblock, BLOCK_SIZE_OS_QUAD);
            }

            if (route[1] < 2)
            {
                mech::accumulate_from_to<BLOCK_SIZE_OS>(tblock, output[0]);
            }
            if (route[1] > 0)
            {
                mech::accumulate_from_to<BLOCK_SIZE_OS>(tblockR, output[1]);
            }
        }
    }

    if (osc1 || ring12)
    {
        if (FMmode == fm_2and3to1)
        {
            mech::add_block<BLOCK_SIZE_OS>(osc[1]->output, osc[2]->output, fmbuffer);
            osc[0]->process_block(
                noteShiftFromPitchParam(
                    (scene->osc[0].keytrack.val.b ? state.pitch : ktrkroot + state.scenepbpitch) +
                        octaveSize * scene->osc[0].octave.val.i,
                    0),
                drift, is_wide, true,
                storage->db_to_linear(localcopy[scene->fm_depth.param_id_in_scene].f));
        }
        else if (FMmode)
        {
            osc[0]->process_block(
                noteShiftFromPitchParam(
                    (scene->osc[0].keytrack.val.b ? state.pitch : ktrkroot + state.scenepbpitch) +
                        octaveSize * scene->osc[0].octave.val.i,
                    0),
                drift, is_wide, true,
                storage->db_to_linear(localcopy[scene->fm_depth.param_id_in_scene].f));
        }
        else
        {
            osc[0]->process_block(
                noteShiftFromPitchParam(
                    (scene->osc[0].keytrack.val.b ? state.pitch : ktrkroot + state.scenepbpitch) +
                        octaveSize * scene->osc[0].octave.val.i,
                    0),
                drift, is_wide);
        }

        if (osc1)
        {
            if (is_wide)
            {
                osclevels[le_osc1].multiply_2_blocks_to(osc[0]->output, osc[0]->outputR, tblock,
                                                        tblockR, BLOCK_SIZE_OS_QUAD);
            }
            else
            {
                osclevels[le_osc1].multiply_block_to(osc[0]->output, tblock, BLOCK_SIZE_OS_QUAD);
            }

            if (route[0] < 2)
            {
                mech::accumulate_from_to<BLOCK_SIZE_OS>(tblock, output[0]);
            }
            if (route[0] > 0)
            {
                mech::accumulate_from_to<BLOCK_SIZE_OS>(tblockR, output[1]);
            }
        }
    }

    if (ring12)
    {
        all_ring_modes_block(osc[0]->output, osc[1]->output, osc[0]->outputR, osc[1]->outputR,
                             tblock, tblockR, is_wide, scene->level_ring_12.deform_type,
                             osclevels[le_ring12], BLOCK_SIZE_OS_QUAD);

        if (route[3] < 2)
        {
            mech::accumulate_from_to<BLOCK_SIZE_OS>(tblock, output[0]);
        }
        if (route[3] > 0)
        {
            mech::accumulate_from_to<BLOCK_SIZE_OS>(tblockR, output[1]);
        }
    }

    if (ring23)
    {
        all_ring_modes_block(osc[1]->output, osc[2]->output, osc[1]->outputR, osc[2]->outputR,
                             tblock, tblockR, is_wide, scene->level_ring_23.deform_type,
                             osclevels[le_ring23], BLOCK_SIZE_OS_QUAD);

        if (route[4] < 2)
        {
            mech::accumulate_from_to<BLOCK_SIZE_OS>(tblock, output[0]);
        }
        if (route[4] > 0)
        {
            mech::accumulate_from_to<BLOCK_SIZE_OS>(tblockR, output[1]);
        }
    }

    if (noise)
    {
        float noisecol = limit_range(localcopy[scene->noise_colour.param_id_in_scene].f, -1.f, 1.f);
        auto is_stereo_noise = scene->noise_colour.deform_type == NoiseColorChannels::STEREO;
        for (int i = 0; i < BLOCK_SIZE_OS; i += 2)
        {
            ((float *)tblock)[i] = sdsp::correlated_noise_o2mk2_supplied_value(
                noisegenL[0], noisegenL[1], noisecol, storage->rand_pm1());
            ((float *)tblock)[i + 1] = ((float *)tblock)[i];
            if (is_wide)
            {
                if (is_stereo_noise)
                {
                    ((float *)tblockR)[i] = sdsp::correlated_noise_o2mk2_supplied_value(
                        noisegenR[0], noisegenR[1], noisecol, storage->rand_pm1());
                    ((float *)tblockR)[i + 1] = ((float *)tblockR)[i];
                }
                else
                {
                    ((float *)tblockR)[i] = ((float *)tblock)[i];
                    ((float *)tblockR)[i + 1] = ((float *)tblock)[i + 1];
                }
            }
        }

        if (is_wide)
        {
            osclevels[le_noise].multiply_2_blocks(tblock, tblockR, BLOCK_SIZE_OS_QUAD);
        }
        else
        {
            osclevels[le_noise].multiply_block(tblock, BLOCK_SIZE_OS_QUAD);
        }

        if (route[5] < 2)
        {
            mech::accumulate_from_to<BLOCK_SIZE_OS>(tblock, output[0]);
        }
        if (route[5] > 0)
        {
            mech::accumulate_from_to<BLOCK_SIZE_OS>(tblockR, output[1]);
        }
    }

    // pre-filter gain
    osclevels[le_pfg].multiply_2_blocks(output[0], output[1], BLOCK_SIZE_OS_QUAD);

    for (int i = 0; i < BLOCK_SIZE_OS; i++)
    {
        _mm_store_ss(((float *)&Q.DL[i] + Qe), _mm_load_ss(&output[0][i]));
        _mm_store_ss(((float *)&Q.DR[i] + Qe), _mm_load_ss(&output[1][i]));
    }
    SetQFB(&Q, Qe);

    age++;
    if (!state.gate)
        age_release++;

    return state.keep_playing;
}

template <bool noLFOSources> void SurgeVoice::applyModulationToLocalcopy()
{
    vector<ModulationRouting>::iterator iter;
    iter = scene->modulation_voice.begin();
    while (iter != scene->modulation_voice.end())
    {
        int src_id = iter->source_id;
        int dst_id = iter->destination_id;
        float depth = iter->depth;

        if (noLFOSources && isLFO((::modsources)src_id))
        {
        }
        else if (modsources[src_id])
        {
            localcopy[dst_id].f +=
                depth * modsources[src_id]->get_output(iter->source_index) * (1.0 - iter->muted);
        }
        iter++;
    }

    if (mpeEnabled)
    {
        // See github issue 1214. This basically compensates for
        // channel AT being per-voice in MPE mode (since it is per channel)
        // vs per-scene (since it is per keyboard in non MPE mode).
        iter = scene->modulation_scene.begin();
        while (iter != scene->modulation_scene.end())
        {
            int src_id = iter->source_id;
            if (src_id == ms_aftertouch && modsources[src_id])
            {
                int dst_id = iter->destination_id;
                // I don't THINK we need this but am not sure the global params are in my localcopy
                // span
                if (dst_id >= 0 && dst_id < n_scene_params)
                {
                    float depth = iter->depth;
                    localcopy[dst_id].f +=
                        depth * modsources[src_id]->get_output(0) * (1.0 - iter->muted);
                }
            }
            iter++;
        }

        monoAftertouchSource.set_target(state.voiceChannelState->pressure +
                                        noteExpressions[PRESSURE]);
        timbreSource.set_target(state.voiceChannelState->timbre + noteExpressions[TIMBRE]);

        if (scene->modsource_doprocess[ms_aftertouch])
        {
            monoAftertouchSource.process_block();
        }
        timbreSource.process_block();

        float bendNormalized = state.voiceChannelState->pitchBend / 8192.f;
        state.mpePitchBend.set_target(bendNormalized);
        state.mpePitchBend.process_block();
    }
    else
    {
        /* When not in MPE mode, channel aftertouch is already smoothed at scene level.
         * Do not smooth it again, force the output to the current value.
         */
        monoAftertouchSource.init(0, state.voiceChannelState->pressure);

        // TimbreSource used to be ignored in non-MPE mode; now it just echose the
        // note expression
        timbreSource.set_target(noteExpressions[TIMBRE]);
        timbreSource.process_block();
    }

    for (int i = 0; i < paramModulationCount; ++i)
    {
        auto &pc = polyphonicParamModulations[i];
        switch (pc.vt_type)
        {
        case vt_float:
            localcopy[pc.param_id].f += pc.value;
            break;
        case vt_int:
        {
            auto v =
                std::clamp((int)(round)(localcopy[pc.param_id].i + pc.value), pc.imin, pc.imax);
            localcopy[pc.param_id].i = v;
            break;
        }
        case vt_bool:
            if (pc.value > 0.5)
                localcopy[pc.param_id].b = true;
            if (pc.value < 0.5)
                localcopy[pc.param_id].b = false;
            break;
        }
    }
}

void SurgeVoice::set_path(bool osc1, bool osc2, bool osc3, int FMmode, bool ring12, bool ring23,
                          bool noise)
{
    this->osc1 = osc1;
    this->osc2 = osc2;
    this->osc3 = osc3;
    this->FMmode = FMmode;
    this->ring12 = ring12;
    this->ring23 = ring23;
    this->noise = noise;
}

void SurgeVoice::SetQFB(QuadFilterChainState *Q, int e) // Q == 0 means init(ialise)
{
    using namespace sst::filters;

    fbq = Q;
    fbqi = e;

    float FMix1, FMix2;
    switch (scene->filterblock_configuration.val.i)
    {
    case fc_serial1:
    case fc_serial2:
    case fc_serial3:
    case fc_ring:
    case fc_wide:
        FMix1 = min(1.f, 1.f - localcopy[id_fbalance].f);
        FMix2 = min(1.f, 1.f + localcopy[id_fbalance].f);
        break;
    default:
        FMix1 = 0.5f - 0.5f * localcopy[id_fbalance].f;
        FMix2 = 0.5f + 0.5f * localcopy[id_fbalance].f;
        break;
    }

    // HERE
    float Drive = db_to_linear(scene->wsunit.drive.get_extended(localcopy[id_drive].f));
    float Gain = db_to_linear(localcopy[id_vca].f +
                              localcopy[id_vcavel].f * (1.f - velocitySource.get_output(0))) *
                 modsources[ms_ampeg]->get_output(0);
    float FB = scene->feedback.get_extended(localcopy[id_feedback].f);

    if (!Q)
    {
        // We need to initialize the waveshaper registers
        for (int c = 0; c < 2; ++c)
            sst::waveshapers::initializeWaveshaperRegister(
                static_cast<sst::waveshapers::WaveshaperType>(scene->wsunit.type.val.i),
                FBP.WS[c].R);
    }

    if (Q)
    {
        set1f(Q->Gain, e, FBP.Gain);
        set1f(Q->dGain, e, (Gain - FBP.Gain) * BLOCK_SIZE_OS_INV);
        set1f(Q->Drive, e, FBP.Drive);
        set1f(Q->dDrive, e, (Drive - FBP.Drive) * BLOCK_SIZE_OS_INV);
        set1f(Q->FB, e, FBP.FB);
        set1f(Q->dFB, e, (FB - FBP.FB) * BLOCK_SIZE_OS_INV);
        set1f(Q->Mix1, e, FBP.Mix1);
        set1f(Q->dMix1, e, (FMix1 - FBP.Mix1) * BLOCK_SIZE_OS_INV);
        set1f(Q->Mix2, e, FBP.Mix2);
        set1f(Q->dMix2, e, (FMix2 - FBP.Mix2) * BLOCK_SIZE_OS_INV);

        for (int c = 0; c < 2; ++c)
        {
            for (int i = 0; i < sst::waveshapers::n_waveshaper_registers; ++i)
            {
                set1f(Q->WSS[c].R[i], e, FBP.WS[c].R[i]);
            }
            set1ui(Q->WSS[c].init, e, 0xFFFFFFFF);
        }
    }

    FBP.Gain = Gain;
    FBP.Drive = Drive;
    FBP.FB = FB;
    FBP.Mix1 = FMix1;
    FBP.Mix2 = FMix2;

    // filterunits
    if (Q)
    {
        set1f(Q->wsLPF, e, FBP.wsLPF); // remember state
        set1f(Q->FBlineL, e, FBP.FBlineL);
        set1f(Q->FBlineR, e, FBP.FBlineR);
        Q->FU[0].active[e] = 0xffffffff;
        Q->FU[1].active[e] = 0xffffffff;
        Q->FU[2].active[e] = 0xffffffff;
        Q->FU[3].active[e] = 0xffffffff;

        float keytrack = state.pitch - (float)scene->keytrack_root.val.i;
        float fenv = modsources[ms_filtereg]->get_output(0);
        float cutoffA =
            localcopy[id_cfa].f + localcopy[id_kta].f * keytrack + localcopy[id_emoda].f * fenv;
        float cutoffB =
            localcopy[id_cfb].f + localcopy[id_ktb].f * keytrack + localcopy[id_emodb].f * fenv;

        if (scene->f2_cutoff_is_offset.val.b)
            cutoffB += cutoffA;

        CM[0].MakeCoeffs(cutoffA, localcopy[id_resoa].f,
                         static_cast<FilterType>(scene->filterunit[0].type.val.i),
                         static_cast<FilterSubType>(scene->filterunit[0].subtype.val.i), storage,
                         scene->filterunit[0].cutoff.extend_range);
        CM[1].MakeCoeffs(
            cutoffB, scene->f2_link_resonance.val.b ? localcopy[id_resoa].f : localcopy[id_resob].f,
            static_cast<FilterType>(scene->filterunit[1].type.val.i),
            static_cast<FilterSubType>(scene->filterunit[1].subtype.val.i), storage,
            scene->filterunit[1].cutoff.extend_range);

        for (int u = 0; u < n_filterunits_per_scene; u++)
        {
            if (scene->filterunit[u].type.val.i != 0)
            {
                CM[u].updateState(Q->FU[u], e);
                for (int i = 0; i < n_filter_registers; i++)
                {
                    set1f(Q->FU[u].R[i], e, FBP.FU[u].R[i]);
                }

                Q->FU[u].DB[e] = FBP.Delay[u];
                Q->FU[u].WP[e] = FBP.FU[u].WP;

                if (scene->filterblock_configuration.val.i == fc_wide)
                {
                    CM[u].updateState(Q->FU[u + 2], e);
                    for (int i = 0; i < n_filter_registers; i++)
                    {
                        set1f(Q->FU[u + 2].R[i], e, FBP.FU[u + 2].R[i]);
                    }

                    Q->FU[u + 2].DB[e] = FBP.Delay[u + 2];
                    Q->FU[u + 2].WP[e] = FBP.FU[u].WP;
                }
            }
        }
    }
}

void SurgeVoice::GetQFB()
{
    using namespace sst::filters;

    for (int u = 0; u < n_filterunits_per_scene; u++)
    {
        if (scene->filterunit[u].type.val.i != 0)
        {
            for (int i = 0; i < n_filter_registers; i++)
            {
                FBP.FU[u].R[i] = get1f(fbq->FU[u].R[i], fbqi);
            }

            for (int i = 0; i < n_cm_coeffs; i++)
            {
                CM[u].C[i] = get1f(fbq->FU[u].C[i], fbqi);
            }
            FBP.FU[u].WP = fbq->FU[u].WP[fbqi];

            if (scene->filterblock_configuration.val.i == fc_wide)
            {
                for (int i = 0; i < n_filter_registers; i++)
                {
                    FBP.FU[u + 2].R[i] = get1f(fbq->FU[u + 2].R[i], fbqi);
                }
            }
        }
    }
    for (int c = 0; c < 2; ++c)
    {
        for (int i = 0; i < sst::waveshapers::n_waveshaper_registers; ++i)
        {
            FBP.WS[c].R[i] = get1f(fbq->WSS[c].R[i], fbqi);
        }
    }
    FBP.FBlineL = get1f(fbq->FBlineL, fbqi);
    FBP.FBlineR = get1f(fbq->FBlineR, fbqi);
    FBP.wsLPF = get1f(fbq->wsLPF, fbqi);
}

void SurgeVoice::freeAllocatedElements()
{
    for (int i = 0; i < n_oscs; ++i)
    {
        osc[i]->~Oscillator();
        osc[i] = nullptr;
        osctype[i] = -1;
    }
    for (int i = 0; i < n_lfos_voice; ++i)
    {
        lfo[i].completedModulation();
    }
}

void SurgeVoice::applyPolyphonicParamModulation(Parameter *p, double value,
                                                double underlyingMonoMod)
{
    // For a discussion of underlyingMonoMod please see the comment in
    // SurgeSynthesizer::applyParameterPolyphonicModulation

    // quickly do a search to see if i'm there
    int param_id = p->param_id_in_scene;
    for (int i = 0; i < paramModulationCount; ++i)
    {
        if (polyphonicParamModulations[i].param_id == param_id)
        {
            auto &pp = polyphonicParamModulations[i];
            pp.vt_type = (valtypes)p->valtype;
            switch (pp.vt_type)
            {
            case vt_float:
                pp.value = value * (p->val_max.f - p->val_min.f);
                break;
            case vt_int:
                pp.value = value * (p->val_max.i - p->val_min.i);
                pp.imin = p->val_min.i;
                pp.imax = p->val_max.i;
                break;
            case vt_bool:
                pp.value = value;
            }

            pp.value -= underlyingMonoMod;
            return;
        }
    }
    int idx = paramModulationCount;
    assert(paramModulationCount < maxPolyphonicParamModulations);
    if (paramModulationCount < maxPolyphonicParamModulations)
    {
        auto &pp = polyphonicParamModulations[idx];
        pp.param_id = param_id;
        pp.vt_type = (valtypes)p->valtype;
        switch (pp.vt_type)
        {
        case vt_float:
            pp.value = value * (p->val_max.f - p->val_min.f);
            break;
        case vt_int:
            pp.value = value * (p->val_max.i - p->val_min.i);
            pp.imin = p->val_min.i;
            pp.imax = p->val_max.i;
            break;
        case vt_bool:
            pp.value = value;
        }

        pp.value -= underlyingMonoMod;
        paramModulationCount++;
    }
}

void SurgeVoice::applyNoteExpression(NoteExpressionType net, float value)
{
    // std::cout << __func__ << _D(net) << _D(value) << std::endl;
    if (net != UNKNOWN)
        noteExpressions[net] = value;
}

void SurgeVoice::retriggerLFOEnvelopes()
{
    for (int i = 0; i < n_lfos_voice; ++i)
    {
        auto &anLfo = lfo[i];
        auto &lfoData = scene->lfo[i];

        if (lfoData.trigmode.val.i == lm_keytrigger)
        {
            anLfo.attack();
        }
        else
        {
            anLfo.retriggerEnvelope();
        }
    }

    int ao = scene->modsources[ms_random_unipolar]->get_active_outputs();
    rndUni.set_active_outputs(ao);
    for (int w = 0; w < ao; ++w)
        rndUni.set_output(w, scene->modsources[ms_random_unipolar]->get_output(w));

    ao = scene->modsources[ms_random_bipolar]->get_active_outputs();
    rndBi.set_active_outputs(ao);
    for (int w = 0; w < ao; ++w)
        rndBi.set_output(w, scene->modsources[ms_random_bipolar]->get_output(w));

    altUni.set_output(0, scene->modsources[ms_alternate_unipolar]->get_output(0));
    altBi.set_output(0, scene->modsources[ms_alternate_bipolar]->get_output(0));
}

void SurgeVoice::resetPortamentoFrom(int key, int channel)
{
    if ((scene->polymode.val.i == pm_mono_st_fp) ||
        (scene->portamento.val.f == scene->portamento.val_min.f))
        state.portasrc_key = state.getPitch(storage);
    else
    {
        float lk = key;
#ifndef SURGE_SKIP_ODDSOUND_MTS
        if (storage->oddsound_mts_client && storage->oddsound_mts_active_as_client)
        {
            lk += MTS_RetuningInSemitones(storage->oddsound_mts_client, lk, channel);
            state.portasrc_key = lk;
        }
        else
#endif
        {
            if (storage->mapChannelToOctave && !mpeEnabled)
                state.portasrc_key =
                    channelKeyEquvialent(lk, state.channel, mpeEnabled, storage, true);
            else
                state.portasrc_key = storage->remapKeyInMidiOnlyMode(lk);
        }
    }
    state.priorpkey = state.portasrc_key;
    state.portaphase = 0;
}

void SurgeVoice::retriggerOSCWithIndependentAttacks()
{
    for (int i = 0; i < n_oscs; ++i)
    {
        if (osc[i])
        {
            // This matches the override in ::process_block
            float ktrkroot = 60;
            auto usep = noteShiftFromPitchParam((scene->osc[i].keytrack.val.b
                                                     ? state.getPitch(storage)
                                                     : ktrkroot + state.scenepbpitch) +
                                                    octaveSize * scene->osc[i].octave.val.i,
                                                0);

            /*
             * This is awfully special case but it's the best solution
             */
            if (scene->osc[i].type.val.i == ot_string)
            {
                osc[i]->init(usep);
            }
            if (scene->osc[i].type.val.i == ot_twist &&
                !scene->osc[i].p[n_osc_params - 2].deactivated)
            {
                osc[i]->init(usep);
            }
        }
    }
}

bool SurgeVoice::matchesChannelKeyId(int16_t channel, int16_t key, int32_t nid)
{
    bool chanMatch{false}, keyMatch{false}, nidMatch{false};
    if (channel == -1)
        chanMatch = true;
    else
        chanMatch = (state.channel == channel);

    if (key == -1)
        keyMatch = true;
    else
        keyMatch = (state.key == key);

    if (nid == -1)
        nidMatch = true;
    else
        nidMatch = (host_note_id == nid);

    return chanMatch && keyMatch && nidMatch;
}
