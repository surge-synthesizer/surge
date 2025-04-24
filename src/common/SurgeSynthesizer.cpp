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

#include "SurgeSynthesizer.h"
#include <fmt/core.h>
#include "DSPUtils.h"
#include <ctime>

#include "SurgeParamConfig.h"

#include "UserDefaults.h"
#include "filesystem/import.h"
#include "Effect.h"
#include "globals.h"

#include <algorithm>
#include <thread>
#include <set>
#ifndef SURGE_SKIP_ODDSOUND_MTS
#include "libMTSClient.h"
#endif

#include "SurgeMemoryPools.h"

#include "sst/basic-blocks/mechanics/block-ops.h"
#include "sst/basic-blocks/dsp/Clippers.h"
#include "sst/cpputils/constructors.h"

using namespace std;
namespace mech = sst::basic_blocks::mechanics;
namespace sdsp = sst::basic_blocks::dsp;
namespace cutl = sst::cpputils;

using CMSKey = ControllerModulationSourceVector<1>; // sigh see #4286 for failed first try

SurgeSynthesizer::SurgeSynthesizer(PluginLayer *parent, const std::string &suppliedDataPath)
    : storage(suppliedDataPath), hpA{cutl::make_array<BiquadFilter, n_hpBQ>(&storage)},
      hpB{cutl::make_array<BiquadFilter, n_hpBQ>(&storage)}, _parent(parent), halfbandA(6, true),
      halfbandB(6, true), halfbandIN(6, true), mpeEnabled(storage.mpeEnabled)
{
    switch_toggled_queued = false;
    audio_processing_active = false;
    halt_engine = false;
    release_if_latched[0] = true;
    release_if_latched[1] = true;
    release_anyway[0] = false;
    release_anyway[1] = false;
    load_fx_needed = true;
    process_input = false; // hosts set this if there are input busses

    fx_suspend_bitmask = 0;

    for (int i = 0; i < n_fx_slots; ++i)
    {
        fx[i].reset(nullptr);
    }

    for (int i = 0; i < disallowedLearnCCs.size(); i++)
    {
        if (i == 0 || i == 6 || i == 32 || i == 38 || i == 64 || i == 74 || (i >= 98 && i <= 101) ||
            i == 120 || i == 123)
        {
            disallowedLearnCCs[i] = true;
        }
    }

    srand((unsigned)time(nullptr));
    // TODO: FIX SCENE ASSUMPTION
    memset(storage.getPatch().scenedata[0], 0, sizeof(pdata) * n_scene_params);
    memset(storage.getPatch().scenedata[1], 0, sizeof(pdata) * n_scene_params);

    memset(storage.getPatch().scenedataOrig[0], 0, sizeof(pdata) * n_scene_params);
    memset(storage.getPatch().scenedataOrig[1], 0, sizeof(pdata) * n_scene_params);

    memset(storage.getPatch().globaldata, 0, sizeof(pdata) * n_global_params);
    memset(mControlInterpolatorUsed, 0, sizeof(bool) * num_controlinterpolators);

    for (int i = 0; i < n_fx_slots; i++)
    {
        fxsync[i] = storage.getPatch().fx[i];
        fx_reload[i] = false;
        fx_reload_mod[i] = false;
    }

    stopSound();

    for (int i = 0; i < MAX_VOICES; i++)
    {
        voices_usedby[0][i] = 0;
        voices_usedby[1][i] = 0;
    }

    for (int sc = 0; sc < n_scenes; sc++)
    {
        FBQ[sc] = new QuadFilterChainState[MAX_VOICES >> 2]();

        for (int i = 0; i < (MAX_VOICES >> 2); ++i)
        {
            InitQuadFilterChainStateToZero(&(FBQ[sc][i]));
        }
    }

    SurgePatch &patch = storage.getPatch();

    storage.smoothingMode = (Modulator::SmoothingMode)(int)Surge::Storage::getUserDefaultValue(
        &storage, Surge::Storage::SmoothingMode, (int)(Modulator::SmoothingMode::LEGACY));
    storage.pitchSmoothingMode = (Modulator::SmoothingMode)(int)Surge::Storage::getUserDefaultValue(
        &storage, Surge::Storage::PitchSmoothingMode, (int)(Modulator::SmoothingMode::DIRECT));

    midiSoftTakeover =
        (bool)Surge::Storage::getUserDefaultValue(&storage, Surge::Storage::MIDISoftTakeover, 0);

    patch.polylimit.val.i = DEFAULT_POLYLIMIT;

    for (int sc = 0; sc < n_scenes; sc++)
    {
        SurgeSceneStorage &scene = patch.scene[sc];
        scene.modsources.resize(n_modsources);

        for (int i = 0; i < n_modsources; i++)
        {
            scene.modsources[i] = 0;
        }

        scene.modsources[ms_modwheel] = new ControllerModulationSource(storage.smoothingMode);
        scene.modsources[ms_breath] = new ControllerModulationSource(storage.smoothingMode);
        scene.modsources[ms_expression] = new ControllerModulationSource(storage.smoothingMode);
        scene.modsources[ms_sustain] = new ControllerModulationSource(storage.smoothingMode);
        scene.modsources[ms_aftertouch] = new ControllerModulationSource(storage.smoothingMode);
        scene.modsources[ms_pitchbend] = new ControllerModulationSource(storage.smoothingMode);
        scene.modsources[ms_lowest_key] = new CMSKey(storage.smoothingMode);
        scene.modsources[ms_highest_key] = new CMSKey(storage.smoothingMode);
        scene.modsources[ms_latest_key] = new CMSKey(storage.smoothingMode);

        ((CMSKey *)scene.modsources[ms_lowest_key])->init(0, 0.f);
        ((CMSKey *)scene.modsources[ms_highest_key])->init(0, 0.f);
        ((CMSKey *)scene.modsources[ms_latest_key])->init(0, 0.f);

        scene.modsources[ms_random_bipolar] = new RandomModulationSource(true);
        scene.modsources[ms_random_unipolar] = new RandomModulationSource(false);
        scene.modsources[ms_alternate_bipolar] = new AlternateModulationSource(true);
        scene.modsources[ms_alternate_unipolar] = new AlternateModulationSource(false);

        for (int osc = 0; osc < n_oscs; osc++)
        {
            // p[5] is unison detune
            scene.osc[osc].p[5].val.f = 0.1f;
        }

        for (int i = 0; i < n_filterunits_per_scene; i++)
        {
            scene.filterunit[i].type.set_user_data(&patch.patchFilterSelectorMapper);
        }

        scene.wsunit.type.set_user_data(&patch.patchWaveshaperSelectorMapper);
        scene.filterblock_configuration.val.i = fc_wide;

        for (int l = 0; l < n_lfos_scene; l++)
        {
            scene.modsources[ms_slfo1 + l] = new LFOModulationSource();
            ((LFOModulationSource *)scene.modsources[ms_slfo1 + l])
                ->assign(&storage, &scene.lfo[n_lfos_voice + l], storage.getPatch().scenedata[sc],
                         0, &patch.stepsequences[sc][n_lfos_voice + l],
                         &patch.msegs[sc][n_lfos_voice + l],
                         &patch.formulamods[sc][n_lfos_voice + l]);
            ((LFOModulationSource *)scene.modsources[ms_slfo1 + l])->setIsVoice(false);
        }

        for (int k = 0; k < 128; ++k)
        {
            midiKeyPressedForScene[sc][k] = 0;
        }
    }

    for (int i = 0; i < n_customcontrollers; i++)
    {
        patch.scene[0].modsources[ms_ctrl1 + i] = new MacroModulationSource(storage.smoothingMode);

        for (int j = 1; j < n_scenes; j++)
        {
            patch.scene[j].modsources[ms_ctrl1 + i] = patch.scene[0].modsources[ms_ctrl1 + i];
        }
    }

    for (int s = 0; s < n_scenes; ++s)
    {
        for (auto ms : patch.scene[s].modsources)
        {
            if (ms)
            {
                ms->set_samplerate(storage.samplerate, storage.samplerate_inv);
            }
        }
    }

    amp.set_blocksize(BLOCK_SIZE);
    amp_mute.set_blocksize(BLOCK_SIZE);

    for (int i = 0; i < n_send_slots; ++i)
    {
        FX[i].set_blocksize(BLOCK_SIZE);
        send[i][0].set_blocksize(BLOCK_SIZE);
        send[i][1].set_blocksize(BLOCK_SIZE);
    }

    storage.activeVoiceCount = 0;
    refresh_editor = false;
    patch_loaded = false;
    storage.getPatch().category = "Init";
    storage.getPatch().name = "Init";
    storage.getPatch().comment = "";
    storage.getPatch().author = "Surge Synth Team";
    storage.getPatch().license = "Licensed under the maximally permissive CC0 license";
    midiprogramshavechanged = false;

    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        input[0][i] = 0.f;
        input[1][i] = 0.f;
    }

    masterfade = 1.f;
    current_category_id = -1;
    patchid_queue = -1;
    has_patchid_file = false;
    patchid_file[0] = 0;
    patchid = -1;
    CC0 = 0;
    CC32 = 0;
    PCH = 0;

    for (int i = 0; i < 8; i++)
    {
        refresh_ctrl_queue[i] = -1;
        refresh_parameter_queue[i] = -1;
    }

    for (int i = 0; i < 8; i++)
    {
        vu_peak[i] = 0.f;
    }

    learn_param_from_cc = -1;
    learn_macro_from_cc = -1;
    learn_param_from_note = -1;

    for (int i = 0; i < 16; i++)
    {
        memset(&channelState[i], 0, sizeof(MidiChannelState));
    }

    mpeEnabled = false;
    mpeVoices = 0;
    storage.mpePitchBendRange =
        (float)Surge::Storage::getUserDefaultValue(&storage, Surge::Storage::MPEPitchBendRange, 48);
    mpeGlobalPitchBendRange = 0;

    int pid = 0;
    patchid_queue = -1;
    has_patchid_file = false;
    bool lookingForFactory = (storage.initPatchCategoryType == "Factory");

    for (auto p : storage.patch_list)
    {
        if (p.name == storage.initPatchName &&
            storage.patch_category[p.category].name == storage.initPatchCategory &&
            storage.patch_category[p.category].isFactory == lookingForFactory)
        {
            patchid_queue = pid;
            break;
        }

        pid++;
    }

    if (patchid_queue >= 0)
    {
        processAudioThreadOpsWhenAudioEngineUnavailable(true); // DANGER MODE IS ON
    }

    patchid_queue = -1;
    has_patchid_file = false;
}

SurgeSynthesizer::~SurgeSynthesizer()
{
    {
        /*
         * This should "never" happen due to cleanup at end of
         * loadPatchInBackgroundThread
         */
        std::lock_guard<std::mutex> mg(patchLoadSpawnMutex);
        if (patchLoadThread)
            patchLoadThread->join();
    }

    stopSound();

    for (int sc = 0; sc < n_scenes; sc++)
    {
        delete[] FBQ[sc];
    }

    for (int sc = 0; sc < n_scenes; sc++)
    {
        delete storage.getPatch().scene[sc].modsources[ms_modwheel];
        delete storage.getPatch().scene[sc].modsources[ms_breath];
        delete storage.getPatch().scene[sc].modsources[ms_expression];
        delete storage.getPatch().scene[sc].modsources[ms_sustain];
        delete storage.getPatch().scene[sc].modsources[ms_aftertouch];
        delete storage.getPatch().scene[sc].modsources[ms_pitchbend];
        delete storage.getPatch().scene[sc].modsources[ms_lowest_key];
        delete storage.getPatch().scene[sc].modsources[ms_highest_key];
        delete storage.getPatch().scene[sc].modsources[ms_latest_key];

        delete storage.getPatch().scene[sc].modsources[ms_random_bipolar];
        delete storage.getPatch().scene[sc].modsources[ms_random_unipolar];
        delete storage.getPatch().scene[sc].modsources[ms_alternate_bipolar];
        delete storage.getPatch().scene[sc].modsources[ms_alternate_unipolar];

        for (int i = 0; i < n_lfos_scene; i++)
        {
            delete storage.getPatch().scene[sc].modsources[ms_slfo1 + i];
        }
    }

    for (int i = 0; i < n_customcontrollers; i++)
    {
        delete storage.getPatch().scene[0].modsources[ms_ctrl1 + i];
    }
}

// A voice is routed to a particular scene if channelmask & n.
// So, "1" means scene A, "2" means scene B and "3" (= 2 | 1) means both.
int SurgeSynthesizer::calculateChannelMask(int channel, int key)
{
    bool useMIDICh2Ch3 = Surge::Storage::getUserDefaultValue(
        &storage, Surge::Storage::UseCh2Ch3ToPlayScenesIndividually, true);

    if (storage.mapChannelToOctave)
    {
        useMIDICh2Ch3 = false;
    }

    int channelmask = channel;

    if (((channel == 0 || channel > 2) && useMIDICh2Ch3) ||
        (channel >= 0 && channel < 16 && !useMIDICh2Ch3) || mpeEnabled ||
        storage.mapChannelToOctave || storage.getPatch().scenemode.val.i == sm_chsplit)
    {
        switch (storage.getPatch().scenemode.val.i)
        {
        case sm_single:
            if (storage.getPatch().scene_active.val.i == 1)
            {
                channelmask = 2;
            }
            else
            {
                channelmask = 1;
            }
            break;
        case sm_dual:
            channelmask = 3;
            break;
        case sm_split:
            if (key < storage.getPatch().splitpoint.val.i)
            {
                channelmask = 1;
            }
            else
            {
                channelmask = 2;
            }
            break;
        case sm_chsplit:
            if (channel < ((int)(storage.getPatch().splitpoint.val.i / 8) + 1))
            {
                channelmask = 1;
            }
            else
            {
                channelmask = 2;
            }

            break;
        }
    }
    else if (storage.getPatch().scenemode.val.i == sm_single)
    {
        if (storage.getPatch().scene_active.val.i == 1)
        {
            channelmask = 2;
        }
        else
        {
            channelmask = 1;
        }
    }

    return channelmask;
}

void SurgeSynthesizer::playNote(char channel, char key, char velocity, char detune,
                                int32_t host_noteid, int32_t forceScene)
{
    if (halt_engine)
    {
        return;
    }

#ifndef SURGE_SKIP_ODDSOUND_MTS
    if (storage.oddsound_mts_client && storage.oddsound_mts_active_as_client)
    {
        if (MTS_ShouldFilterNote(storage.oddsound_mts_client, key, channel))
        {
            return;
        }
    }
#endif

    if (learn_param_from_note >= 0 &&
        storage.getPatch().param_ptr[learn_param_from_note]->ctrltype == ct_midikey_or_channel)
    {
        const auto sm = storage.getPatch().scenemode.val.i;

        if (sm == scene_mode::sm_split)
        {
            storage.getPatch().param_ptr[learn_param_from_note]->val.i = key;
            refresh_editor = true;
        }

        if (sm == scene_mode::sm_chsplit)
        {
            storage.getPatch().param_ptr[learn_param_from_note]->val.i = channel * 8;
            refresh_editor = true;
        }

        learn_param_from_note = -1;

        return;
    }

    midiNoteEvents++;

    if (!storage.isStandardTuning)
    {
        if (!storage.currentTuning.isMidiNoteMapped(key))
        {
            return;
        }
    }

    // For split/dual
    // MIDI Channel 1 plays the split/dual
    // MIDI Channel 2 plays A
    // MIDI Channel 3 plays B

    int channelmask = calculateChannelMask(channel, key);
    if (forceScene == 0)
        channelmask = 1;
    if (forceScene == 1)
        channelmask = 2;

    // TODO: FIX SCENE ASSUMPTION
    if (channelmask & 1)
    {
        midiKeyPressedForScene[0][key] = ++orderedMidiKey;
        playVoice(0, channel, key, velocity, detune, host_noteid);
    }
    if (channelmask & 2)
    {
        midiKeyPressedForScene[1][key] = ++orderedMidiKey;
        playVoice(1, channel, key, velocity, detune, host_noteid);
    }

    channelState[channel].keyState[key].keystate = velocity;
    channelState[channel].keyState[key].lastdetune = detune;
    channelState[channel].keyState[key].lastNoteIdForKey = host_noteid;

    /*
    ** OK so why is there hold stuff here? This is play not release.
    ** Well if you release a key with the pedal down it goes into the
    ** 'release me later' buffer. If you press the key again it stays there
    ** so even with the key held, you end up releasing it when you pedal.
    **
    ** Or: NoteOn / Pedal On / Note Off / Note On / Pedal Off should leave the note ringing
    **
    ** and right now it doesn't
    */
    bool noHold = !channelState[channel].hold;

    if (mpeEnabled)
        noHold = noHold && !channelState[0].hold;

    if (!noHold)
    {
        for (int sc = 0; sc < n_scenes; ++sc)
        {
            for (auto &h : holdbuffer[sc])
            {
                if (h.channel == channel && h.key == key)
                {
                    h.channel = -1;
                    h.key = -1;
                    h.originalChannel = channel;
                    h.originalKey = key;
                }
            }
        }
    }
}

// This supports an OSC message that specifies pitch by frequency (rather than by MIDI note number)
void SurgeSynthesizer::playNoteByFrequency(float freq, char velocity, int32_t id)
{
    auto k = 12 * log2(freq / 440) + 69;
    auto mk = (int)std::floor(k);
    auto off = k - mk;
    // std::cout << "playNote freq: " << freq << "  note: " << mk << "  offset: " << off
    //    << "  vel: " << static_cast<int>(velocity) << " id : " << id << std::endl;
    playNote(0, mk, velocity, 0, id);
    // and now to fix the tuning
    this->setNoteExpression(SurgeVoice::PITCH, id, mk, 0, off); // since PITCH is in semitones
}

void SurgeSynthesizer::softkillVoice(int s)
{
    list<SurgeVoice *>::iterator iter, max_playing, max_released;
    int max_age = -1, max_age_release = -1;
    iter = voices[s].begin();

    while (iter != voices[s].end())
    {
        SurgeVoice *v = *iter;
        assert(v);
        if (v->state.gate)
        {
            if (v->age > max_age)
            {
                max_age = v->age;
                max_playing = iter;
            }
        }
        else if (!v->state.uberrelease)
        {
            if (v->age_release > max_age_release)
            {
                max_age_release = v->age_release;
                max_released = iter;
            }
        }
        iter++;
    }
    if (max_age_release >= 0)
        (*max_released)->uber_release();
    else if (max_age >= 0)
        (*max_playing)->uber_release();
}

// only allow 'margin' number of voices to be softkilled simultaneously
void SurgeSynthesizer::enforcePolyphonyLimit(int s, int margin)
{
    list<SurgeVoice *>::iterator iter;

    int paddedPoly = std::min((storage.getPatch().polylimit.val.i + margin), MAX_VOICES - 1);
    if (voices[s].size() > paddedPoly)
    {
        int excess_voices = max(0, (int)voices[s].size() - paddedPoly);
        iter = voices[s].begin();

        while (iter != voices[s].end())
        {
            if (excess_voices < 1)
                break;

            SurgeVoice *v = *iter;
            if (v->state.uberrelease)
            {
                excess_voices--;
                freeVoice(v);
                iter = voices[s].erase(iter);
            }
            else
                iter++;
        }
    }
}

int SurgeSynthesizer::getNonUltrareleaseVoices(int s) const
{
    int count = 0;

    auto iter = voices[s].begin();
    while (iter != voices[s].end())
    {
        SurgeVoice *v = *iter;
        assert(v);
        if (!v->state.uberrelease)
            count++;
        iter++;
    }

    return count;
}

int SurgeSynthesizer::getNonReleasedVoices(int s) const
{
    int count = 0;

    auto iter = voices[s].begin();
    while (iter != voices[s].end())
    {
        SurgeVoice *v = *iter;
        assert(v);
        if (v->state.gate)
            count++;
        iter++;
    }

    return count;
}

SurgeVoice *SurgeSynthesizer::getUnusedVoice(int scene)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (!voices_usedby[scene][i])
        {
            voices_usedby[scene][i] = scene + 1;
            return &voices_array[scene][i];
        }
    }
    return 0;
}

void SurgeSynthesizer::freeVoice(SurgeVoice *v)
{
    if (v->host_note_id >= 0)
    {
        bool used_away = false;
        // does any other voice have this voiceid
        for (int s = 0; s < n_scenes; ++s)
        {
            for (auto vo : voices[s])
            {
                if (vo != v && vo->host_note_id == v->host_note_id)
                {
                    used_away = true;
                }
            }
        }
        if (!used_away)
        {
            notifyEndedNote(v->host_note_id, v->originating_host_key, v->originating_host_channel);
        }
    }

    int foundScene{-1}, foundIndex{-1};
    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (voices_usedby[0][i] && (v == &voices_array[0][i]))
        {
            assert(foundScene == -1);
            assert(foundIndex == -1);
            foundScene = 0;
            foundIndex = i;
            voices_usedby[0][i] = 0;
        }
        if (voices_usedby[1][i] && (v == &voices_array[1][i]))
        {
            assert(foundScene == -1);
            assert(foundIndex == -1);
            foundScene = 1;
            foundIndex = i;
            voices_usedby[1][i] = 0;
        }
    }
    v->freeAllocatedElements();

    /*
     * Call the SurgeVoice destructor here. But since SurgeVoice lives in an
     * array it will be destroyed on exit also so re-construct it with the
     * default on the same meory just to be pedantic
     */
    v->~SurgeVoice();
    v = new (&voices_array[foundScene][foundIndex]) SurgeVoice();
}

void SurgeSynthesizer::notifyEndedNote(int32_t nid, int16_t key, int16_t chan, bool thisBlock)
{
    if (!doNotifyEndedNote)
        return;
    if (thisBlock)
    {
        int h = hostNoteEndedDuringBlockCount;
        endedHostNoteIds[h] = nid;
        endedHostNoteOriginalKey[h] = key;
        endedHostNoteOriginalChannel[h] = chan;
        hostNoteEndedDuringBlockCount++;
    }
    else
    {
        int h = hostNoteEndedToPushToNextBlock;
        nextBlockEndedHostNoteIds[h] = nid;
        nextBlockEndedHostNoteOriginalKey[h] = key;
        nextBlockEndedHostNoteOriginalChannel[h] = chan;
        hostNoteEndedToPushToNextBlock++;
    }
}

int SurgeSynthesizer::getMpeMainChannel(int voiceChannel, int key)
{
    if (mpeEnabled)
    {
        return 0;
    }

    return voiceChannel;
}

void SurgeSynthesizer::playVoice(int scene, char channel, char key, char velocity, char detune,
                                 int32_t host_noteid, int16_t override_hostkey,
                                 int16_t override_hostchan)
{
    int16_t host_originating_key = (int16_t)key;
    int16_t host_originating_channel = (int16_t)channel;

    if (override_hostkey >= 0)
        host_originating_key = override_hostkey;
    if (override_hostchan >= 0)
        host_originating_channel = override_hostchan;

    if (getNonReleasedVoices(scene) == 0)
    {
        for (int l = 0; l < n_lfos_scene; l++)
        {
            if (storage.getPatch().scene[scene].lfo[n_lfos_voice + l].shape.val.i == lt_formula)
            {
                auto lms = dynamic_cast<LFOModulationSource *>(
                    storage.getPatch().scene[scene].modsources[ms_slfo1 + l]);
                if (lms)
                {
                    Surge::Formula::setupEvaluatorStateFrom(lms->formulastate, storage.getPatch(),
                                                            scene);
                }
            }
            storage.getPatch().scene[scene].modsources[ms_slfo1 + l]->attack();
        }
    }

    for (int i = ms_random_bipolar; i <= ms_alternate_unipolar; ++i)
    {
        storage.getPatch().scene[scene].modsources[i]->attack();
    }

    int excessVoices =
        max(0, (int)getNonUltrareleaseVoices(scene) - storage.getPatch().polylimit.val.i + 1);

    for (int i = 0; i < excessVoices; i++)
    {
        softkillVoice(scene);
    }
    enforcePolyphonyLimit(scene, 3);

    int lowkey = 0, hikey = 127;
    if (storage.getPatch().scenemode.val.i == sm_split)
    {
        if (scene == 0)
            hikey = storage.getPatch().splitpoint.val.i - 1;
        else
            lowkey = storage.getPatch().splitpoint.val.i;
    }

    int lowMpeChan = 1; // skip control chan
    int highMpeChan = 16;
    if (storage.getPatch().scenemode.val.i == sm_chsplit)
    {
        if (scene == 0)
        {
            highMpeChan = (int)(storage.getPatch().splitpoint.val.i / 8 + 1);
        }
        else
        {
            lowMpeChan = (int)(storage.getPatch().splitpoint.val.i / 8 + 1);
        }
    }

    auto mode = storage.getPatch().scene[scene].polymode.val.i;
    switch (mode)
    {
    case pm_poly:
    {
        auto m = storage.getPatch().scene[scene].polyVoiceRepeatedKeyMode;
        bool reusedVoice{false};
        if (m != NEW_VOICE_EVERY_NOTEON)
        {
            for (auto v : voices[scene])
            {
                if (v->state.key == key)
                {
                    reclaimVoiceFor(v, key, channel, velocity, scene, host_noteid,
                                    host_originating_channel, host_originating_key, false);
                    reusedVoice = true;
                }
            }
        }

        if (!reusedVoice)
        {
            SurgeVoice *nvoice = getUnusedVoice(scene);

            if (nvoice)
            {
                // This is a constructed voice which we are going to re-construct
                // so run the destructor. See also the handling in freeVoice below
                nvoice->~SurgeVoice();

                int mpeMainChannel = getMpeMainChannel(channel, key);

                voices[scene].push_back(nvoice);
                new (nvoice) SurgeVoice(
                    &storage, &storage.getPatch().scene[scene], storage.getPatch().scenedata[scene],
                    storage.getPatch().scenedataOrig[scene], key, velocity, channel, scene, detune,
                    &channelState[channel].keyState[key], &channelState[mpeMainChannel],
                    &channelState[channel], mpeEnabled, voiceCounter++, host_noteid,
                    host_originating_key, host_originating_channel, 0.f, 0.f);
            }
        }
        break;
    }
    case pm_mono:
    case pm_mono_fp:
    case pm_latch:
    {
        list<SurgeVoice *>::const_iterator iter;
        bool glide = false;

        int primode = storage.getPatch().scene[scene].monoVoicePriorityMode;
        bool createVoice = true;
        if (primode == ALWAYS_HIGHEST || primode == ALWAYS_LOWEST)
        {
            /*
             * There is a chance we don't want to make a voice
             */
            if (mpeEnabled)
            {
                for (int k = lowkey; k < hikey; ++k)
                {
                    for (int mpeChan = lowMpeChan; mpeChan < highMpeChan; ++mpeChan)
                    {
                        if (channelState[mpeChan].keyState[k].keystate)
                        {
                            if (primode == ALWAYS_HIGHEST && k > key)
                                createVoice = false;
                            if (primode == ALWAYS_LOWEST && k < key)
                                createVoice = false;
                        }
                    }
                }
            }
            else if (storage.mapChannelToOctave)
            {
                auto keyadj = SurgeVoice::channelKeyEquvialent(key, channel, mpeEnabled, &storage);

                for (int k = lowkey; k < hikey; ++k)
                {
                    for (int ch = 0; ch < 16; ++ch)
                    {
                        if (channelState[ch].keyState[k].keystate)
                        {
                            auto kadj =
                                SurgeVoice::channelKeyEquvialent(k, ch, mpeEnabled, &storage);

                            if (primode == ALWAYS_HIGHEST && kadj > keyadj)
                                createVoice = false;
                            if (primode == ALWAYS_LOWEST && kadj < keyadj)
                                createVoice = false;
                        }
                    }
                }
            }
            else
            {
                for (int k = lowkey; k < hikey; ++k)
                {
                    if (channelState[channel].keyState[k].keystate)
                    {
                        if (primode == ALWAYS_HIGHEST && k > key)
                            createVoice = false;
                        if (primode == ALWAYS_LOWEST && k < key)
                            createVoice = false;
                    }
                }
            }
        }

        if (createVoice)
        {
            int32_t noteIdToReuse = -1;
            int16_t channelToReuse, keyToReuse;
            SurgeVoice *stealEnvelopesFrom{nullptr};
            bool wasGated{false};
            float pkeyToReuse{0.f}, pphaseToReuse{0.f};
            for (iter = voices[scene].begin(); iter != voices[scene].end(); iter++)
            {
                SurgeVoice *v = *iter;
                if (v->state.scene_id == scene)
                {
                    if (v->state.gate)
                    {
                        glide = true;
                        noteIdToReuse = v->host_note_id;
                        channelToReuse = v->originating_host_channel;
                        keyToReuse = v->originating_host_key;
                        stealEnvelopesFrom = v;
                        if (v->state.portaphase < 1 && (v->state.portasrc_key != v->state.pkey))
                        {
                            pkeyToReuse = v->state.pkey;
                            pphaseToReuse = v->state.portaphase;
                        }
                        wasGated = true;
                    }
                    else
                    {
                        // Non-gated voices only win if there's no gated voice
                        if (!stealEnvelopesFrom)
                        {
                            stealEnvelopesFrom = v;
                            wasGated = false;
                        }
                    }
                    v->uber_release();
                }
            }

            if (stealEnvelopesFrom &&
                storage.getPatch().scene[scene].monoVoiceEnvelopeMode != RESTART_FROM_ZERO)
            {
                reclaimVoiceFor(stealEnvelopesFrom, key, channel, velocity, scene, host_noteid,
                                host_originating_channel, host_originating_key);
                if (mode == pm_mono_fp && !wasGated)
                {
                    stealEnvelopesFrom->resetPortamentoFrom(key, channel);
                }
            }
            else
            {
                if (noteIdToReuse >= 0)
                {
                    // We kill the note id on the *old* voice not the *new* one on this
                    // mode since this mode is supposed to trigger envelopes on keypress
                    notifyEndedNote(host_noteid, noteIdToReuse, channelToReuse, keyToReuse);
                }

                SurgeVoice *nvoice = getUnusedVoice(scene);
                float aegReuse{0.f}, fegReuse{0.f};

                if (nvoice)
                {
                    int mpeMainChannel = getMpeMainChannel(channel, key);

                    voices[scene].push_back(nvoice);
                    if ((storage.getPatch().scene[scene].polymode.val.i == pm_mono_fp) && !glide)
                        storage.last_key[scene] = key;
                    new (nvoice) SurgeVoice(
                        &storage, &storage.getPatch().scene[scene],
                        storage.getPatch().scenedata[scene],
                        storage.getPatch().scenedataOrig[scene], key, velocity, channel, scene,
                        detune, &channelState[channel].keyState[key], &channelState[mpeMainChannel],
                        &channelState[channel], mpeEnabled, voiceCounter++, host_noteid,
                        host_originating_key, host_originating_channel, aegReuse, fegReuse);

                    if (wasGated && pkeyToReuse > 0)
                    {
                        // This is commented out since it runs away in some
                        // cases. It is an attempt to fix #7301

                        // In this case we want to continue the preivously
                        // initiated porta
                        // nvoice->state.pkey = pkeyToReuse;
                        // nvoice->state.portaphase = 1 - pphaseToReuse;
                    }
                }
            }
        }
        else
        {
            /*
             * We still need to indicate that this is an ordered voice even though we don't create
             * it
             */
            channelState[channel].keyState[key].voiceOrder = voiceCounter++;
        }
    }
    break;
    case pm_mono_st:
    case pm_mono_st_fp:
    {
        bool found_one = false;
        int primode = storage.getPatch().scene[scene].monoVoicePriorityMode;
        bool createVoice = true;
        if (primode == ALWAYS_HIGHEST || primode == ALWAYS_LOWEST)
        {
            /*
             * There is a chance we don't want to make a voice
             */
            if (mpeEnabled)
            {
                for (int k = lowkey; k < hikey; ++k)
                {
                    for (int mpeChan = lowMpeChan; mpeChan < highMpeChan; ++mpeChan)
                    {
                        if (channelState[mpeChan].keyState[k].keystate)
                        {
                            if (primode == ALWAYS_HIGHEST && k > key)
                                createVoice = false;
                            if (primode == ALWAYS_LOWEST && k < key)
                                createVoice = false;
                        }
                    }
                }
            }
            else if (storage.mapChannelToOctave)
            {
                auto keyadj = SurgeVoice::channelKeyEquvialent(key, channel, mpeEnabled, &storage);

                for (int k = lowkey; k < hikey; ++k)
                {
                    for (int ch = 0; ch < 16; ++ch)
                    {
                        if (channelState[ch].keyState[k].keystate)
                        {
                            auto kadj =
                                SurgeVoice::channelKeyEquvialent(k, ch, mpeEnabled, &storage);

                            if (primode == ALWAYS_HIGHEST && kadj > keyadj)
                                createVoice = false;
                            if (primode == ALWAYS_LOWEST && kadj < keyadj)
                                createVoice = false;
                        }
                    }
                }
            }
            else
            {
                for (int k = lowkey; k < hikey; ++k)
                {
                    if (channelState[channel].keyState[k].keystate)
                    {
                        if (primode == ALWAYS_HIGHEST && k > key)
                            createVoice = false;
                        if (primode == ALWAYS_LOWEST && k < key)
                            createVoice = false;
                    }
                }
            }
        }

        if (createVoice)
        {
            list<SurgeVoice *>::const_iterator iter;
            SurgeVoice *recycleThis{nullptr};
            float aegStart{0.}, fegStart{0.};
            for (iter = voices[scene].begin(); iter != voices[scene].end(); iter++)
            {
                SurgeVoice *v = *iter;
                if ((v->state.scene_id == scene) && (v->state.gate))
                {
                    v->legato(key, velocity, detune);
                    found_one = true;
                    if (mpeEnabled || storage.mapChannelToOctave)
                    {
                        /*
                        ** This voice was created on a channel but is being legato held to another
                        *channel
                        ** so it needs to borrow the channel and channelState. Obviously this can
                        *only
                        ** happen in MPE mode or channel to octave mode
                        */
                        v->state.channel = channel;
                        v->state.voiceChannelState = &channelState[channel];
                    }
                    break;
                }
                else
                {
                    if (v->state.scene_id == scene && !v->state.uberrelease)
                    {
                        if (storage.getPatch().scene[scene].monoVoiceEnvelopeMode !=
                            RESTART_FROM_ZERO)
                            recycleThis = v;
                        else
                            v->uber_release(); // make this optional for poly legato
                    }
                }
            }
            if (recycleThis)
            {
                reclaimVoiceFor(recycleThis, key, channel, velocity, scene, host_noteid,
                                host_originating_channel, host_originating_key);
            }
            else if (!found_one)
            {
                int mpeMainChannel = getMpeMainChannel(channel, key);

                SurgeVoice *nvoice = getUnusedVoice(scene);
                if (nvoice)
                {
                    voices[scene].push_back(nvoice);
                    new (nvoice) SurgeVoice(
                        &storage, &storage.getPatch().scene[scene],
                        storage.getPatch().scenedata[scene],
                        storage.getPatch().scenedataOrig[scene], key, velocity, channel, scene,
                        detune, &channelState[channel].keyState[key], &channelState[mpeMainChannel],
                        &channelState[channel], mpeEnabled, voiceCounter++, host_noteid,
                        host_originating_key, host_originating_channel, aegStart, fegStart);
                }
            }
            else
            {
                /*
                 * Here we are legato sliding a voice which is fine but it means we
                 * don't create a new voice. That will screw up the voice counters
                 * - basically all voices will have the same count - so a concept of
                 * latest doesn't work. Thus update the channel state making this key
                 * 'newer' than the prior voice (as is normally done in the SurgeVoice
                 * constructor call).
                 */
                channelState[channel].keyState[key].voiceOrder = voiceCounter++;

                /*
                 * but we also need to terminate this voiceid
                 */
                if (host_noteid >= 0)
                {
                    notifyEndedNote(host_noteid, host_originating_key, host_originating_channel,
                                    false);
                }
            }
        }
        else
        {
            /*
             * Still need to update the counter if we don't create in hilow mode
             */
            channelState[channel].keyState[key].voiceOrder = voiceCounter++;
        }
    }
    break;
    }

    updateHighLowKeys(scene);
}

void SurgeSynthesizer::releaseScene(int s)
{
    list<SurgeVoice *>::const_iterator iter;
    for (iter = voices[s].begin(); iter != voices[s].end(); iter++)
    {
        freeVoice(*iter);
    }
    voices[s].clear();

    for (int i = 0; i < n_hpBQ; ++i)
    {
        if (s == 0)
            hpA[i].suspend();
        else
            hpB[i].suspend();
    }
    if (s == 0)
        halfbandA.reset();
    if (s == 1)
        halfbandB.reset();
    halfbandIN.reset();
}

void SurgeSynthesizer::chokeNote(int16_t channel, int16_t key, char velocity, int32_t host_noteid)
{
    /*
     * The strategy here is pretty simple. Do a release note, then go find any voice
     * that matches me and do an uber-release. There may be some wierdo MPE mono
     * choke drum edge case this won't work with but in the bitwig drum machine at
     * least it works great!
     */
    releaseNote(channel, key, velocity, host_noteid);

    for (int sc = 0; sc < n_scenes; ++sc)
    {
        for (auto *v : voices[sc])
        {
            if (v->matchesChannelKeyId(channel, key, host_noteid))
            {
                v->uber_release();
            }
        }
    }
}

void SurgeSynthesizer::releaseNote(char channel, char key, char velocity, int32_t host_noteid)
{
    midiNoteEvents++;
    bool foundVoice[n_scenes];
    for (int sc = 0; sc < n_scenes; ++sc)
    {
        foundVoice[sc] = false;
        for (auto *v : voices[sc])
        {
            foundVoice[sc] = true;
            if ((v->state.key == key) && (v->state.channel == channel) &&
                (host_noteid < 0 || v->host_note_id == host_noteid))
                v->state.releasevelocity = velocity;
        }
    }

    /*
     * Update the midi key for scene
     */
    auto scmd = (scene_mode)storage.getPatch().scenemode.val.i;
    switch (scmd)
    {
    case sm_single:
    {
        auto sc = storage.getPatch().scene_active.val.i;
        midiKeyPressedForScene[sc][key] = 0;
        break;
    }
    case sm_dual:
    {
        for (int i = 0; i < n_scenes; ++i)
            midiKeyPressedForScene[i][key] = 0;
        break;
    }
    case sm_split:
    {
        auto splitkey = storage.getPatch().splitpoint.val.i;
        if (key < splitkey)
        {
            midiKeyPressedForScene[0][key] = 0;
        }
        else
        {
            midiKeyPressedForScene[1][key] = 0;
        }

        break;
    }
    case sm_chsplit:
    {
        auto splitChan = (int)(storage.getPatch().splitpoint.val.i / 8 + 1);
        if (channel < splitChan)
        {
            midiKeyPressedForScene[0][key] = 0;
        }
        else
        {
            midiKeyPressedForScene[1][key] = 0;
        }
        break;
    }
    default:
        break;
    }

    bool noHold = !channelState[channel].hold;
    if (mpeEnabled)
        noHold = noHold && !channelState[0].hold;

    for (int sc = 0; sc < n_scenes; ++sc)
    {
        bool sceneNoHold = noHold;
        auto pm = storage.getPatch().scene[sc].polymode.val.i;
        if (!sceneNoHold && !mpeEnabled && storage.monoPedalMode == RELEASE_IF_OTHERS_HELD &&
            (pm == pm_mono || pm == pm_mono_fp || pm == pm_mono_st || pm == pm_mono_st_fp))
        {
            /*
             * OK so we have a key we are about to put in the hold buffer. BUT in the
             * RELEASE_IF_OTHERS_HELD mode we don't want to hold it if we have other keys
             * down. So do we?
             */
            for (auto k = 127; k >= 0; k--) // search downwards
            {
                if (k != key && channelState[channel].keyState[k].keystate)
                {
                    sceneNoHold =
                        true; // This effects a release of current key because another key is down
                }
            }
        }

        if (sceneNoHold)
            releaseNotePostHoldCheck(sc, channel, key, velocity, host_noteid);
        else
            holdbuffer[sc].push_back(HoldBufferItem{
                channel, key, channel, key, host_noteid}); // hold pedal is down, add to buffer
    }
}

void SurgeSynthesizer::releaseNotePostHoldCheck(int scene, char channel, char key, char velocity,
                                                int32_t host_noteid)
{
    channelState[channel].keyState[key].keystate = 0;
    list<SurgeVoice *>::const_iterator iter;
    for (int s = 0; s < n_scenes; s++)
    {
        bool do_switch = false;
        int k = 0;

        for (iter = voices[scene].begin(); iter != voices[scene].end(); iter++)
        {
            SurgeVoice *v = *iter;
            int lowkey = 0, hikey = 127;
            if (storage.getPatch().scenemode.val.i == sm_split)
            {
                if (v->state.scene_id == 0)
                    hikey = storage.getPatch().splitpoint.val.i - 1;
                else
                    lowkey = storage.getPatch().splitpoint.val.i;
            }

            int lowMpeChan = 1; // Control chan gets skipped
            int highMpeChan = 16;
            if (storage.getPatch().scenemode.val.i == sm_chsplit)
            {
                if (v->state.scene_id == 0)
                {
                    highMpeChan = (int)(storage.getPatch().splitpoint.val.i / 8 + 1);
                }
                else
                {
                    lowMpeChan = (int)(storage.getPatch().splitpoint.val.i / 8 + 1);
                }
            }

            auto polymode = storage.getPatch().scene[v->state.scene_id].polymode.val.i;
            switch (polymode)
            {
            case pm_poly:
            {
                if ((v->state.key == key) && (v->state.channel == channel) && (v->state.gate) &&
                    (host_noteid < 0 || v->host_note_id == host_noteid))
                {
                    v->release();
                }
            }
            break;
            case pm_mono:
            case pm_mono_fp:
            case pm_latch:
            {

                /*
                ** In these modes, our job when we release a note is to see if
                ** any other note is held.
                **
                ** In normal MIDI mode, that means scanning the keystate of our
                ** channel looking for another note.
                **
                ** In MPE mode, where each note is per channel, that means
                ** scanning all non-main channels rather than ourself for the
                ** highest note
                *
                * But with the introduction of voice modes finding the next one
                * is trickier so add these two little lambdas
                */

                if ((v->state.key == key) && (v->state.channel == channel))
                {
                    int activateVoiceKey = 60, activateVoiceChannel = 0; // these will be overridden
                    auto priorityMode =
                        storage.getPatch().scene[v->state.scene_id].monoVoicePriorityMode;

                    // v->release();
                    if (!mpeEnabled && !storage.mapChannelToOctave)
                    {
                        int highest = -1, lowest = 128, latest = -1;
                        int64_t lt = 0;
                        for (k = hikey; k >= lowkey && !do_switch; k--) // search downwards
                        {
                            if (channelState[channel].keyState[k].keystate)
                            {
                                if (k >= highest)
                                    highest = k;
                                if (k <= lowest)
                                    lowest = k;
                                if (channelState[channel].keyState[k].voiceOrder >= lt)
                                {
                                    latest = k;
                                    lt = channelState[channel].keyState[k].voiceOrder;
                                }
                            }
                        }

                        switch (storage.getPatch().scene[v->state.scene_id].monoVoicePriorityMode)
                        {
                        case ALWAYS_HIGHEST:
                        case NOTE_ON_LATEST_RETRIGGER_HIGHEST:
                            k = highest >= 0 ? highest : -1;
                            break;
                        case ALWAYS_LATEST:
                            k = latest >= 0 ? latest : -1;
                            break;
                        case ALWAYS_LOWEST:
                            k = lowest <= 127 ? lowest : -1;
                            break;
                        }

                        if (k >= 0)
                        {
                            do_switch = true;
                            activateVoiceKey = k;
                            activateVoiceChannel = channel;
                        }
                    }
                    else if (!mpeEnabled && storage.mapChannelToOctave)
                    {
                        auto keyadj =
                            SurgeVoice::channelKeyEquvialent(key, channel, mpeEnabled, &storage);

                        int highest = -1, lowest = 128, latest = -1;
                        int highestadj = -100, lowestadj = 1000;
                        int highchan = 0, lowchan = 0, latechan = 0;
                        int64_t lt = 0;

                        for (int k = lowkey; k < hikey; ++k)
                        {
                            for (int ch = 0; ch < 16; ++ch)
                            {
                                if (channelState[ch].keyState[k].keystate)
                                {
                                    auto kadj = SurgeVoice::channelKeyEquvialent(k, ch, mpeEnabled,
                                                                                 &storage);
                                    if (kadj >= highestadj)
                                    {
                                        /*
                                         * Why k and not kadj here? Well this is the key which maps
                                         * to the highest voice, but when we play it we play it as
                                         * if we had pressed the key. So we want to *search* in
                                         * adjusted space then *play* in keyboard space otherwise we
                                         * will apply the adjustment twice.
                                         */
                                        highest = k;
                                        highchan = ch;
                                        highestadj = kadj;
                                    }
                                    if (kadj <= lowestadj)
                                    {
                                        lowest = k;
                                        lowchan = ch;
                                        lowestadj = kadj;
                                    }
                                    if (channelState[channel].keyState[k].voiceOrder >= lt)
                                    {
                                        latest = k;
                                        latechan = ch;
                                        lt = channelState[channel].keyState[k].voiceOrder;
                                    }
                                }
                            }
                        }

                        int ch = channel;
                        switch (storage.getPatch().scene[v->state.scene_id].monoVoicePriorityMode)
                        {
                        case ALWAYS_HIGHEST:
                        case NOTE_ON_LATEST_RETRIGGER_HIGHEST:
                            k = highest >= 0 ? highest : -1;
                            ch = highchan;
                            break;
                        case ALWAYS_LATEST:
                            k = latest >= 0 ? latest : -1;
                            ch = latechan;
                            break;
                        case ALWAYS_LOWEST:
                            k = lowest <= 127 ? lowest : -1;
                            ch = lowchan;
                            break;
                        }

                        if (k >= 0)
                        {
                            do_switch = true;
                            activateVoiceKey = k;
                            activateVoiceChannel = ch;
                        }
                    }
                    else
                    { // MPE branch
                        int highest = -1, lowest = 128, latest = -1;
                        int hichan{-1}, lowchan{-1}, latechan{-1};
                        int64_t lt = 0;

                        for (k = hikey; k >= lowkey && !do_switch; k--)
                        {
                            for (int mpeChan = lowMpeChan; mpeChan < highMpeChan; ++mpeChan)
                            {
                                if (mpeChan != channel &&
                                    channelState[mpeChan].keyState[k].keystate)
                                {
                                    if (k >= highest)
                                    {
                                        highest = k;
                                        hichan = mpeChan;
                                    }
                                    if (k <= lowest)
                                    {
                                        lowest = k;
                                        lowchan = mpeChan;
                                    }
                                    if (channelState[mpeChan].keyState[k].voiceOrder >= lt)
                                    {
                                        lt = channelState[mpeChan].keyState[k].voiceOrder;
                                        latest = k;
                                        latechan = mpeChan;
                                    }
                                }
                            }
                        }
                        k = -1;
                        int kchan;

                        switch (storage.getPatch().scene[v->state.scene_id].monoVoicePriorityMode)
                        {
                        case ALWAYS_HIGHEST:
                        case NOTE_ON_LATEST_RETRIGGER_HIGHEST:
                            k = highest >= 0 ? highest : -1;
                            kchan = hichan;
                            break;
                        case ALWAYS_LATEST:
                            k = latest >= 0 ? latest : -1;
                            kchan = latechan;
                            break;
                        case ALWAYS_LOWEST:
                            k = lowest <= 127 ? lowest : -1;
                            kchan = lowchan;
                            break;
                        }

                        if (k >= 0)
                        {
                            do_switch = true;
                            activateVoiceChannel = kchan;
                            activateVoiceKey = k;
                        }
                    }

                    if (!do_switch)
                    {
                        if (storage.getPatch().scene[v->state.scene_id].polymode.val.i != pm_latch)
                            v->release();
                    }
                    else
                    {
                        /*
                         * We are about to *play* a voice to replace the old voice but our
                         * velocity here is our release velocity. So grab the prior notes
                         * velocity since it is the best we have at this point.
                         */
                        auto priorNoteVel =
                            std::clamp((char)(v->modsources[ms_velocity]->get_output(0) * 128),
                                       (char)0, (char)127);
                        v->uber_release();

                        // confirm that no notes are active
                        if (getNonUltrareleaseVoices(scene) == 0)
                        {
                            /* We need to find that last voice if we are done to restart in FP
                             * so fake a gate quickly. See #4971
                             */
                            v->state.gate = (polymode == pm_mono_fp);
                            doNotifyEndedNote = false;
                            playVoice(scene, activateVoiceChannel, activateVoiceKey,
                                      priorNoteVel /* not velocity! */,
                                      channelState[activateVoiceChannel].keyState[k].lastdetune,
                                      v->host_note_id, v->originating_host_key,
                                      v->originating_host_channel);
                            doNotifyEndedNote = true;
                            v->state.gate = false;
                        }
                    }
                }
            }
            break;
            case pm_mono_st:
            case pm_mono_st_fp:
            {
                /*
                ** In these modes the note will collide on the main channel
                */
                int stateChannel = getMpeMainChannel(v->state.channel, v->state.key);
                int noteChannel = getMpeMainChannel(channel, key);

                if ((v->state.key == key) && (stateChannel == noteChannel))
                {
                    bool do_release = true;
                    /*
                    ** Again the note I need to legato to is on my channel in non MPE and
                    ** is on another channel in MPE mode
                    */
                    if (!mpeEnabled && !storage.mapChannelToOctave)
                    {
                        int highest = -1, lowest = 128, latest = -1;
                        int64_t lt = 0;
                        for (k = hikey; k >= lowkey; k--) // search downwards
                        {
                            if (k != key && channelState[channel].keyState[k].keystate)
                            {
                                if (k >= highest)
                                    highest = k;
                                if (k <= lowest)
                                    lowest = k;
                                if (channelState[channel].keyState[k].voiceOrder >= lt)
                                {
                                    latest = k;
                                    lt = channelState[channel].keyState[k].voiceOrder;
                                }
                            }
                        }

                        switch (storage.getPatch().scene[v->state.scene_id].monoVoicePriorityMode)
                        {
                        case ALWAYS_HIGHEST:
                        case NOTE_ON_LATEST_RETRIGGER_HIGHEST:
                            k = highest >= 0 ? highest : -1;
                            break;
                        case ALWAYS_LATEST:
                            k = latest >= 0 ? latest : -1;
                            break;
                        case ALWAYS_LOWEST:
                            k = lowest <= 127 ? lowest : -1;
                            break;
                        }

                        if (k >= 0)
                        {
                            v->legato(k, velocity, channelState[channel].keyState[k].lastdetune);
                            do_release = false;
                        }
                    }
                    else if (!mpeEnabled && storage.mapChannelToOctave)
                    {
                        auto keyadj =
                            SurgeVoice::channelKeyEquvialent(key, channel, mpeEnabled, &storage);

                        int highest = -1, lowest = 128, latest = -1;
                        int highestadj = -1000, lowestadj = 1000;
                        int highchan = 0, lowchan = 0, latechan = 0;
                        int64_t lt = 0;

                        for (int k = lowkey; k < hikey; ++k)
                        {
                            for (int ch = 0; ch < 16; ++ch)
                            {
                                if (channelState[ch].keyState[k].keystate)
                                {
                                    auto kadj = SurgeVoice::channelKeyEquvialent(k, ch, mpeEnabled,
                                                                                 &storage);

                                    if (kadj >= highestadj)
                                    {
                                        highest = k; // this is not kadj on purpose. See above.
                                        highchan = ch;
                                        highestadj = kadj;
                                    }
                                    if (kadj <= lowestadj)
                                    {
                                        lowest = k;
                                        lowchan = ch;
                                        lowestadj = kadj;
                                    }
                                    if (channelState[channel].keyState[k].voiceOrder >= lt)
                                    {
                                        latest = k;
                                        latechan = ch;
                                        lt = channelState[channel].keyState[k].voiceOrder;
                                    }
                                }
                            }
                        }

                        int ch = channel;
                        switch (storage.getPatch().scene[v->state.scene_id].monoVoicePriorityMode)
                        {
                        case ALWAYS_HIGHEST:
                        case NOTE_ON_LATEST_RETRIGGER_HIGHEST:
                            k = highest >= 0 ? highest : -1;
                            ch = highchan;
                            break;
                        case ALWAYS_LATEST:
                            k = latest >= 0 ? latest : -1;
                            ch = latechan;
                            break;
                        case ALWAYS_LOWEST:
                            k = lowest <= 127 ? lowest : -1;
                            ch = lowchan;
                            break;
                        }

                        if (k >= 0)
                        {
                            v->legato(k, velocity, channelState[ch].keyState[k].lastdetune);
                            do_release = false;

                            v->state.channel = ch;
                            v->state.voiceChannelState = &channelState[ch];
                        }
                    }
                    else
                    {
                        int highest = -1, lowest = 128, latest = -1;
                        int hichan{-1}, lowchan{-1}, latechan{-1};
                        int64_t lt = 0;

                        for (k = hikey; k >= lowkey && !do_switch; k--)
                        {
                            for (int mpeChan = lowMpeChan; mpeChan < highMpeChan; ++mpeChan)
                            {
                                if (mpeChan != channel &&
                                    channelState[mpeChan].keyState[k].keystate)
                                {
                                    if (k >= highest)
                                    {
                                        highest = k;
                                        hichan = mpeChan;
                                    }
                                    if (k <= lowest)
                                    {
                                        lowest = k;
                                        lowchan = mpeChan;
                                    }
                                    if (channelState[mpeChan].keyState[k].voiceOrder >= lt)
                                    {
                                        lt = channelState[mpeChan].keyState[k].voiceOrder;
                                        latest = k;
                                        latechan = mpeChan;
                                    }
                                }
                            }
                        }
                        k = -1;
                        int kchan;

                        switch (storage.getPatch().scene[v->state.scene_id].monoVoicePriorityMode)
                        {
                        case ALWAYS_HIGHEST:
                        case NOTE_ON_LATEST_RETRIGGER_HIGHEST:
                            k = highest >= 0 ? highest : -1;
                            kchan = hichan;
                            break;
                        case ALWAYS_LATEST:
                            k = latest >= 0 ? latest : -1;
                            kchan = latechan;
                            break;
                        case ALWAYS_LOWEST:
                            k = lowest <= 127 ? lowest : -1;
                            kchan = lowchan;
                            break;
                        }

                        if (k >= 0)
                        {
                            v->legato(k, velocity, channelState[kchan].keyState[k].lastdetune);
                            do_release = false;
                            // See the comment above at the other _st legato spot
                            v->state.channel = kchan;
                            v->state.voiceChannelState = &channelState[kchan];
                            // std::cout << _D(v->state.gate) << _D(v->state.key) <<
                            // _D(v->state.scene_id ) << std::endl;
                        }
                    }

                    if (do_release)
                        v->release();
                }
            }
            break;
            };
        }
    }

    updateHighLowKeys(scene);

    // Used to be a scene loop here
    {
        if (getNonReleasedVoices(scene) == 0)
        {
            for (int l = 0; l < n_lfos_scene; l++)
                storage.getPatch().scene[scene].modsources[ms_slfo1 + l]->release();
        }
    }
}

void SurgeSynthesizer::releaseNoteByHostNoteID(int32_t host_noteid, char velocity)
{
    std::array<uint16_t, 128> done;
    std::fill(done.begin(), done.end(), 0);

    bool found{false};
    for (int s = 0; s < n_scenes; ++s)
    {
        auto &ptS = storage.getPatch().scene[s];

        // In the single trigger mode we can recycle ids
        // so we need to portentialy kill the originating
        // note also
        bool recycleNoteID =
            ptS.polymode.val.i == pm_mono_st_fp || ptS.polymode.val.i == pm_mono_st;
        for (auto v : voices[s])
        {
            if (v->host_note_id == host_noteid)
            {
                found = true;
                done[v->state.key] |= 1 << v->state.channel;
                if (recycleNoteID)
                    done[v->originating_host_key] |= 1 << v->state.channel;
            }
        }
    }

    int nidx{0};
    for (auto d : done)
    {
        if (d)
        {
            for (auto ch = 0; ch < 16; ++ch)
            {
                if (d & 1 << ch)
                {
                    releaseNote(ch, nidx, velocity, host_noteid);
                }
            }
        }
        nidx++;
    }

    if (!found)
    {
        // go looking in the keystate in case it is a termianted voice
        for (int ch = 0; ch < 15; ++ch)
        {
            for (int k = 0; k < 128; ++k)
            {
                if (channelState[ch].keyState[k].lastNoteIdForKey == host_noteid)
                {
                    releaseNote(ch, k, velocity, host_noteid);
                }
            }
        }
    }
}

void SurgeSynthesizer::setNoteExpression(SurgeVoice::NoteExpressionType net, int32_t note_id,
                                         int16_t key, int16_t channel, float value)
{
    for (int sc = 0; sc < n_scenes; sc++)
    {
        for (auto v : voices[sc])
        {
            if (v->matchesChannelKeyId(channel, key, note_id))
            {
                v->applyNoteExpression(net, value);
            }
        }
    }
}

void SurgeSynthesizer::updateHighLowKeys(int scene)
{
    static constexpr bool resetToZeroOnLastRelease = false;

    float ktRoot = (float)storage.getPatch().scene[scene].keytrack_root.val.i;
    float twelfth = 1.f / 12.f;

    int highest = -1, lowest = 129, latest = -1, latestC = 0;
    for (int k = 0; k < 128; ++k)
    {
        if (midiKeyPressedForScene[scene][k] > 0)
        {
            highest = std::max(k, highest);
            lowest = std::min(k, lowest);
        }
        if (midiKeyPressedForScene[scene][k] > latestC)
        {
            latestC = midiKeyPressedForScene[scene][k];
            latest = k;
        }
    }

    float highestP = highest, lowestP = lowest, latestP = latest;
    for (auto *v : voices[scene])
    {
        auto pitch = v->state.pkey;
        auto vkey = v->state.key;
        highestP = std::max(pitch, highestP);
        lowestP = std::min(pitch, lowestP);
        if (vkey == latest)
            latestP = pitch;
    }

    if (lowest < 129)
        ((CMSKey *)storage.getPatch().scene[scene].modsources[ms_lowest_key])
            ->init(0, (lowest - ktRoot) * twelfth);
    else if (resetToZeroOnLastRelease)
        ((CMSKey *)storage.getPatch().scene[scene].modsources[ms_lowest_key])->init(0, 0.f);

    if (highest >= 0)
    {
        ((CMSKey *)storage.getPatch().scene[scene].modsources[ms_highest_key])
            ->init(0, (highest - ktRoot) * twelfth);
    }
    else if (resetToZeroOnLastRelease)
    {
        ((CMSKey *)storage.getPatch().scene[scene].modsources[ms_highest_key])->init(0, 0.f);
    }

    if (latest >= 0)
    {
        ((CMSKey *)storage.getPatch().scene[scene].modsources[ms_latest_key])
            ->init(0, (latest - ktRoot) * twelfth);
    }
    else if (resetToZeroOnLastRelease)
    {
        ((CMSKey *)storage.getPatch().scene[scene].modsources[ms_latest_key])->init(0, 0.f);
    }
}

// negative value will reset all channels, otherwise resets pitch bend on specified channel only
void SurgeSynthesizer::resetPitchBend(int8_t channel)
{
    storage.pitch_bend = 0.f;
    pitchbendMIDIVal = 0;
    hasUpdatedMidiCC = true;

    if (channel > -1)
    {
        channelState[channel].pitchBend = 0;
    }
    else
    {
        for (int i = 0; i < 16; i++)
        {
            channelState[i].pitchBend = 0;
        }
    }

    for (int sc = 0; sc < n_scenes; sc++)
    {
        ((ControllerModulationSource *)storage.getPatch().scene[sc].modsources[ms_pitchbend])
            ->set_target(storage.pitch_bend);
    }
}

void SurgeSynthesizer::pitchBend(char channel, int value)
{
    if (mpeEnabled && channel != 0)
    {
        channelState[channel].pitchBend = value;

        /*
        ** TODO: Handling of channel 0 and mpeGlobalPitchBendRange was broken with the addition of
        ** smoothing. We should probably add that back, if it turns out someone actually uses it :)
        **
        ** Currently, channelState[].pitchBendInSemitones is unused, but it hasn't been removed from
        ** the code yet for this reason.
        **
        ** For now, we ignore channel zero here so it functions like the old code did in practice
        ** when mpeGlobalPitchBendRange remained at zero.
        */
    }

    /*
    ** So here's the thing. We want global pitch bend modulation to work for other things in MPE
    ** mode. This code has been here forever. But that means we need to ignore the channel[0] MPE
    ** pitchbend elsewhere, especially since the range was hardwired to 2 (but is now 0).
    ** As far as I know, the main MPE devices don't have a global pitch bend anyway, so this just
    ** screws up regular keyboards sending channel 0 pitch bend in MPE mode.
    */

    if (!mpeEnabled || channel == 0)
    {
        storage.pitch_bend = value / 8192.f;

        pitchbendMIDIVal = value;
        hasUpdatedMidiCC = true;

        for (int sc = 0; sc < n_scenes; sc++)
        {
            ((ControllerModulationSource *)storage.getPatch().scene[sc].modsources[ms_pitchbend])
                ->set_target(storage.pitch_bend);
        }
    }
}

void SurgeSynthesizer::channelAftertouch(char channel, int value)
{
    float fval = (float)value / 127.f;

    channelState[channel].pressure = fval;

    if (!mpeEnabled || channel == 0)
    {
        for (int sc = 0; sc < n_scenes; sc++)
        {
            ((ControllerModulationSource *)storage.getPatch().scene[sc].modsources[ms_aftertouch])
                ->set_target(fval);
        }
    }
}

void SurgeSynthesizer::polyAftertouch(char channel, int key, int value)
{
    float fval = (float)value / 127.f;
    storage.poly_aftertouch[0][channel][key & 127] = fval;
    storage.poly_aftertouch[1][channel][key & 127] = fval;
}

void SurgeSynthesizer::programChange(char channel, int value)
{
    PCH = value;

    auto pid = storage.patchIdToMidiBankAndProgram[CC0][PCH];
    if (pid >= 0)
        patchid_queue = pid;
}

void SurgeSynthesizer::updateDisplay()
{
    // This used to (in VST2) call updateDisplay but that did an Audio -> UI thread cross. Just mark
    // the flag as true
    refresh_editor = true;
}

void SurgeSynthesizer::sendParameterAutomation(long index, float value)
{
    ID eid;
    if (!fromSynthSideId(index, eid))
        return;

    getParent()->surgeParameterUpdated(eid, value);
}

void SurgeSynthesizer::onRPN(int channel, int lsbRPN, int msbRPN, int lsbValue, int msbValue)
{
    /* The MPE specification v1.0 section 2.1.1 says the RPN message format for MPE Configuration
       Message is:

          Bn 64 06
          Bn 65 00
          Bn 06 mm

       where n = 0 is lower zone, n=F is upper zone, all others are invalid
       mm=0 means no MPE and mm=1->F is zone.
    */

    if (lsbRPN == 0 && msbRPN == 0) // pitch bend range
    {
        if (channel == 1)
        {
            storage.mpePitchBendRange = msbValue;
        }
        else if (channel == 0)
        {
            mpeGlobalPitchBendRange = msbValue;
        }
    }
    else if (lsbRPN == 6 && msbRPN == 0) // MPE mode
    {
        mpeEnabled = msbValue > 0;
        mpeVoices = msbValue & 0xF;

        if (storage.mpePitchBendRange < 0.0f)
        {
            storage.mpePitchBendRange = Surge::Storage::getUserDefaultValue(
                &storage, Surge::Storage::MPEPitchBendRange, 48);
        }

        mpeGlobalPitchBendRange = 0;

        return;
    }
    else // we don't support any other RPN message at this time
    {
        return;
    }
}

void SurgeSynthesizer::onNRPN(int channel, int lsbNRPN, int msbNRPN, int lsbValue, int msbValue) {}

float int7ToBipolarFloat(int x)
{
    if (x > 64)
    {
        return (x - 64) * (1.f / 63.f);
    }
    else if (x < 64)
    {
        return (x - 64) * (1.f / 64.f);
    }

    return 0.f;
}

void SurgeSynthesizer::channelController(char channel, int cc, int value)
{
    float fval = (float)value * (1.f / 127.f);

    // store all possible NRPN & RPNs in a short array... amounts to 128 KB or thereabouts
    switch (cc)
    {
    case 0:
        CC0 = value;
        return;
    case 1:
        for (int sc = 0; sc < n_scenes; sc++)
        {
            ((ControllerModulationSource *)storage.getPatch().scene[sc].modsources[ms_modwheel])
                ->set_target(fval);
        }

        modwheelCC = value;
        hasUpdatedMidiCC = true;
        break;
    case 2:
        for (int sc = 0; sc < n_scenes; sc++)
        {
            ((ControllerModulationSource *)storage.getPatch().scene[sc].modsources[ms_breath])
                ->set_target(fval);
        }
        break;
    case 6:
        if (channelState[channel].nrpn_last)
        {
            channelState[channel].nrpn_v[1] = value;

            onNRPN(channel, channelState[channel].nrpn[0], channelState[channel].nrpn[1],
                   channelState[channel].nrpn_v[0], channelState[channel].nrpn_v[1]);
        }
        else
        {
            channelState[channel].rpn_v[1] = value;

            onRPN(channel, channelState[channel].rpn[0], channelState[channel].rpn[1],
                  channelState[channel].rpn_v[0], channelState[channel].rpn_v[1]);
        }
        return;
    case 10:
    {
        if (mpeEnabled)
        {
            channelState[channel].pan = int7ToBipolarFloat(value);
            return;
        }
        break;
    }
    case 11:
        for (int sc = 0; sc < n_scenes; sc++)
        {
            ((ControllerModulationSource *)storage.getPatch().scene[sc].modsources[ms_expression])
                ->set_target(fval);
        }
        break;
    case 32:
        CC32 = value;
        return;
    case 38:
        if (channelState[channel].nrpn_last)
            channelState[channel].nrpn_v[0] = value;
        else
            channelState[channel].rpn_v[0] = value;
        break;
    case 64:
    {
        for (int sc = 0; sc < n_scenes; sc++)
        {
            ((ControllerModulationSource *)storage.getPatch().scene[sc].modsources[ms_sustain])
                ->set_target(fval);
        }

        sustainpedalCC = value;
        hasUpdatedMidiCC = true;

        if (storage.mapChannelToOctave)
        {
            for (int c = 0; c < 16; c++)
            {
                channelState[c].hold = value > 63;
            }
        }
        else
        {
            channelState[channel].hold = value > 63;
        }

        // OK in single mode, only purge scene 0, but in split or dual purge both, and in chsplit
        // pick based on channel
        switch (storage.getPatch().scenemode.val.i)
        {
        case sm_single:
            purgeHoldbuffer(storage.getPatch().scene_active.val.i);
            break;
        case sm_split:
        case sm_dual:
            purgeHoldbuffer(0);
            purgeHoldbuffer(1);
            break;
        case sm_chsplit:
            if (mpeEnabled && channel == 0) // a control channel message
            {
                purgeHoldbuffer(0);
                purgeHoldbuffer(1);
            }
            else
            {
                if (channel < ((int)(storage.getPatch().splitpoint.val.i / 8) + 1))
                    purgeHoldbuffer(0);
                else
                    purgeHoldbuffer(1);
            }
            break;
        }

        return;
    }
    case 74:
    {
        if (mpeEnabled)
        {
            channelState[channel].timbre =
                mpeTimbreIsUnipolar ? (value / 127.f) : int7ToBipolarFloat(value);
            return;
        }
        break;
    }
    case 98: // NRPN LSB
        channelState[channel].nrpn[0] = value;
        channelState[channel].nrpn_last = true;
        return;
    case 99: // NRPN MSB
        channelState[channel].nrpn[1] = value;
        channelState[channel].nrpn_last = true;
        return;
    case 100: // RPN LSB
        channelState[channel].rpn[0] = value;
        channelState[channel].nrpn_last = false;
        return;
    case 101: // RPN MSB
        channelState[channel].rpn[1] = value;
        channelState[channel].nrpn_last = false;
        return;
    case 120: // all sound off
    {
        // make sure we only do All Sound Off over the global MPE channel
        if (!(mpeEnabled && channel != 0))
            allSoundOff();
        return;
    }

    case 123: // all notes off
    {
        // make sure we only do All Notes Off over the global MPE channel
        if (!(mpeEnabled && channel != 0))
            allNotesOff();
        return;
    }
    };

    int cc_encoded = cc;

    // handle RPN/NRPNs (untested)
    if (cc == 6 || cc == 38)
    {
        int tv, cnum;

        if (channelState[channel].nrpn_last)
        {
            tv = (channelState[channel].nrpn_v[1] << 7) + channelState[channel].nrpn_v[0];
            cnum = (channelState[channel].nrpn[1] << 7) + channelState[channel].nrpn[0];
            cc_encoded = cnum | (1 << 16);
        }
        else
        {
            tv = (channelState[channel].rpn_v[1] << 7) + channelState[channel].rpn_v[0];
            cnum = (channelState[channel].rpn[1] << 7) + channelState[channel].rpn[0];
            cc_encoded = cnum | (2 << 16);
        }

        fval = (float)tv / 16384.0f;
        // int cmode = channelState[channel].nrpn_last;
    }

    if (learn_param_from_cc >= 0)
    {
        if (!disallowedLearnCCs.test(cc))
        {
            storage.getPatch().param_ptr[learn_param_from_cc]->midictrl = cc_encoded;
            storage.getPatch().param_ptr[learn_param_from_cc]->midichan = channel;
            storage.getPatch().param_ptr[learn_param_from_cc]->miditakeover_status = sts_locked;

            learn_param_from_cc = -1;
        }
    }

    if ((learn_macro_from_cc >= 0) && (learn_macro_from_cc < n_customcontrollers))
    {
        if (!disallowedLearnCCs.test(cc))
        {
            storage.controllers[learn_macro_from_cc] = cc_encoded;
            storage.controllers_chan[learn_macro_from_cc] = channel;

            learn_macro_from_cc = -1;
        }
    }

    for (int i = 0; i < n_customcontrollers; i++)
    {
        if (storage.controllers[i] == cc_encoded &&
            (storage.controllers_chan[i] == channel || storage.controllers_chan[i] == -1))
        {
            ((ControllerModulationSource *)storage.getPatch().scene[0].modsources[ms_ctrl1 + i])
                ->set_target01(0, fval);
        }
    }

    for (int i = 0; i < (n_global_params + (n_scene_params * n_scenes)); i++)
    {
        auto p = storage.getPatch().param_ptr[i];

        if (p->midictrl == cc_encoded && (p->midichan == channel || p->midichan == -1))
        {
            bool applyControl{true};
            if (midiSoftTakeover && p->miditakeover_status != sts_locked)
            {
                const auto pval = p->get_value_f01();
                /*
                std::cout << "Takeover " << p->get_full_name() << " " << pval << " " << fval
                          << " " << p->miditakeover_status
                          << std::endl;
                          */

                static constexpr float buffer = {1.5f / 127.f}; // 1.5 midi CCs away

                switch (p->miditakeover_status)
                {
                case sts_waiting_for_first_look:
                    if (fval < pval - buffer)
                    {
                        // printf("wait for val below\n");
                        p->miditakeover_status = sts_waiting_below;
                    }
                    else if (fval > pval + buffer)
                    {
                        // printf("wait for val above\n");
                        p->miditakeover_status = sts_waiting_above;
                    }
                    else
                    {
                        // printf("wait for val locked\n");
                        p->miditakeover_status = sts_locked;
                    }
                    break;
                case sts_waiting_below:
                    if (fval > pval - buffer)
                    {
                        // printf("waiting below locked\n");
                        p->miditakeover_status = sts_locked;
                    }
                    break;
                case sts_waiting_above:
                    if (fval < pval + buffer)
                    {
                        // printf("waiting above locked\n");
                        p->miditakeover_status = sts_locked;
                    }
                    break;
                default:
                    break;
                }

                if (p->miditakeover_status != sts_locked)
                {
                    // printf("not locked\n");
                    applyControl = false;
                }
            }

            if (applyControl)
            {
                // std::cout << "About to set parameter to " << fval << std::endl;

                this->setParameterSmoothed(i, fval);

                // Notify audio thread param change listeners (OSC, e.g.)
                // (which run on juce messenger thread)
                for (const auto &it : audioThreadParamListeners)
                    (it.second)(storage.getPatch().param_ptr[i]->oscName, fval, "");

                int j = 0;
                while (j < 7)
                {
                    if ((refresh_ctrl_queue[j] > -1) && (refresh_ctrl_queue[j] != i))
                    {
                        j++;
                    }
                    else
                    {
                        break;
                    }
                }

                refresh_ctrl_queue[j] = i;
                refresh_ctrl_queue_value[j] = fval;
            }
        }
    }
}

void SurgeSynthesizer::allSoundOff()
{
    approachingAllSoundOff = true;
    masterfade = 1.f;
}

void SurgeSynthesizer::allNotesOff()
{
    /*
     * For now, until we move to a sightly less delicate voice manager,
     * do two things here. First run over all the keys we have pressed. Then
     * run over all the voices we have gated. Release both. This deals with both
     * midi and generative patterns in poly and mono mode. Probably a bit overkill
     * but hey that's OK.
     */
    std::set<int> heldNotes;

    for (int sc = 0; sc < n_scenes; sc++)
    {
        for (int key = 0; key < 128; key++)
        {
            if (midiKeyPressedForScene[sc][key])
            {
                heldNotes.insert(key);
            }
        }
    }

    for (auto n : heldNotes)
    {
        for (int ch = 0; ch < 16; ch++)
        {
            if (channelState[ch].keyState[n].keystate > 0)
            {
                releaseNote(ch, n, 0);
            }
        }
    }

    std::vector<std::tuple<int, int, int>> gatedChannelKeyID;
    for (const auto &sceneVoices : voices)
    {
        for (const auto &v : sceneVoices)
        {
            if (v->state.gate)
            {
                gatedChannelKeyID.emplace_back(v->state.channel, v->state.key, v->host_note_id);
            }
        }
    }

    for (auto &[ch, n, id] : gatedChannelKeyID)
    {
        releaseNote(ch, n, 0, id);
    }

#if DEBUG_ALL_VOICES_OFF
    for (const auto &sceneVoices : voices)
    {
        for (const auto &v : sceneVoices)
        {
            if (v->state.gate)
            {
                std::cout << "After all notes off voice is still gated " << (int)v->state.channel
                          << " " << (int)v->state.key << std::endl;
            }
        }
    }
#endif
}

void SurgeSynthesizer::purgeHoldbuffer(int scene)
{
    std::list<HoldBufferItem> retainBuffer;

    for (const auto &hp : holdbuffer[scene])
    {
        auto channel = hp.channel;
        auto key = hp.key;

        if (channel < 0 || key < 0)
        {
            /* this is the 'tricky repeated repease while hold is down' case.
             * In the mono modes the right thing happens (we have a pile of tests)
             * and in mpe it can't happen because each note is on a different channel
             * (in theory) but in poly mode it can.
             */
            auto polymode = storage.getPatch().scene[scene].polymode.val.i;
            if (polymode == pm_poly && !mpeEnabled)
            {
                // The mpe and mono modes and latch have a variety of very difficult handlings
                purgeDuplicateHeldVoicesInPolyMode(scene, hp.originalChannel, hp.originalKey);
            }
        }
        else
        {
            if (!channelState[0].hold && !channelState[channel].hold)
            {
                releaseNotePostHoldCheck(scene, channel, key, 127, hp.host_noteid);
            }
            else
            {
                retainBuffer.push_back(hp);
            }
        }
    }
    holdbuffer[scene] = retainBuffer;
}

void SurgeSynthesizer::purgeDuplicateHeldVoicesInPolyMode(int scene, int channel, int key)
{
    /* If we end up here we know there's multiple voices in the voice structure on this key and
     * channel probably
     */
    std::vector<SurgeVoice *> candidates;
    for (const auto &v : voices[scene])
    {
        if (v->state.key == key && v->state.channel == channel && v->state.gate)
        {
            candidates.push_back(v);
        }
    }
    if (candidates.size() > 1)
    {
        // make sure latest is first
        std::sort(candidates.begin(), candidates.end(), [](const auto &a, const auto &b) {
            return a->state.voiceOrderAtCreate > b->state.voiceOrderAtCreate;
        });
        bool r = false;
        for (auto &v : candidates)
        {
            if (r)
                v->release();
            r = true;
        }
    }
}

void SurgeSynthesizer::stopSound()
{
    for (int i = 0; i < 16; i++)
    {
        channelState[i].hold = false;

        for (int k = 0; k < 128; k++)
        {
            channelState[i].keyState[k].keystate = 0;
            channelState[i].keyState[k].lastdetune = 0;
            channelState[i].keyState[k].lastNoteIdForKey = -1;
        }
    }

    for (int s = 0; s < n_scenes; s++)
    {
        list<SurgeVoice *>::const_iterator iter;
        for (iter = voices[s].begin(); iter != voices[s].end(); iter++)
        {
            freeVoice(*iter);
        }
        voices[s].clear();
    }
    holdbuffer[0].clear();
    holdbuffer[1].clear();
    halfbandA.reset();
    halfbandB.reset();
    halfbandIN.reset();

    for (int i = 0; i < n_hpBQ; i++)
    {
        hpA[i].suspend();
        hpB[i].suspend();
    }

    for (int i = 0; i < n_fx_slots; i++)
    {
        if (fx[i])
        {
            fx[i]->suspend();
        }
    }

    memset(mControlInterpolatorUsed, 0, sizeof(bool) * num_controlinterpolators);
}

void SurgeSynthesizer::setSamplerate(float sr)
{
    storage.setSamplerate(sr);

    for (const auto &f : fx)
    {
        if (f)
        {
            f->sampleRateReset();
        }
    }

    for (int s = 0; s < n_scenes; s++)
    {
        for (auto *v : voices[s])
        {
            assert(v);
            v->sampleRateReset();
        }
    }
}

//-------------------------------------------------------------------------------------------------

int SurgeSynthesizer::GetFreeControlInterpolatorIndex()
{
    for (int i = 0; i < num_controlinterpolators; i++)
    {
        if (!mControlInterpolatorUsed[i])
        {
            return i;
        }
    }
    assert(0);
    return -1;
}

//-------------------------------------------------------------------------------------------------

int SurgeSynthesizer::GetControlInterpolatorIndex(int Id)
{
    for (int i = 0; i < num_controlinterpolators; i++)
    {
        if (mControlInterpolatorUsed[i] && mControlInterpolator[i].id == Id)
        {
            return i;
        }
    }
    return -1;
}

//-------------------------------------------------------------------------------------------------

void SurgeSynthesizer::ReleaseControlInterpolator(int Id)
{
    int Index = GetControlInterpolatorIndex(Id);
    if (Index >= 0)
    {
        assert(Index < num_controlinterpolators);
        mControlInterpolatorUsed[Index] = false;
    }
}

//-------------------------------------------------------------------------------------------------

ControllerModulationSource *SurgeSynthesizer::ControlInterpolator(int Id)
{
    int Index = GetControlInterpolatorIndex(Id);

    if (Index < 0)
        return nullptr;

    return &mControlInterpolator[Index];
}

//-------------------------------------------------------------------------------------------------

ControllerModulationSource *SurgeSynthesizer::AddControlInterpolator(int Id, bool &AlreadyExisted)
{
    int Index = GetControlInterpolatorIndex(Id);

    AlreadyExisted = false;

    if (Index >= 0)
    {
        // Already exists, return it
        AlreadyExisted = true;
        mControlInterpolator[Index].set_samplerate(storage.samplerate, storage.samplerate_inv);
        return &mControlInterpolator[Index];
    }

    Index = GetFreeControlInterpolatorIndex();

    if (Index >= 0)
    {
        // Add new
        mControlInterpolator[Index].id = Id;
        mControlInterpolatorUsed[Index] = true;

        mControlInterpolator[Index].set_samplerate(storage.samplerate, storage.samplerate_inv);
        mControlInterpolator[Index].smoothingMode = storage.smoothingMode; // IMPLEMENT THIS HERE
        return &mControlInterpolator[Index];
    }

    return nullptr;
}

//-------------------------------------------------------------------------------------------------

void SurgeSynthesizer::setParameterSmoothed(long index, float value)
{
    bool AlreadyExisted;
    ControllerModulationSource *mc = AddControlInterpolator(index, AlreadyExisted);
    storage.getPatch().isDirty = true;

    if (mc)
    {
        if (!AlreadyExisted)
        {
            float oldval = storage.getPatch().param_ptr[index]->get_value_f01();
            mc->init(oldval);
            mc->set_target(value);
        }
        else
        {
            mc->set_target(value);
        }
    }
}

//-------------------------------------------------------------------------------------------------

bool SurgeSynthesizer::setParameter01(long index, float value, bool external, bool force_integer)
{
    // does the parameter exist in the interpolator array? If it does, delete it
    ReleaseControlInterpolator(index);
    bool need_refresh = false;

    if (index >= 0 && index < storage.getPatch().param_ptr.size())
    {
        pdata oldval, newval;
        oldval.i = storage.getPatch().param_ptr[index]->val.i;

        storage.getPatch().param_ptr[index]->set_value_f01(value, force_integer);

        {
            auto p = storage.getPatch().param_ptr[index];
            if (p->val.i != oldval.i)
            {
                storage.getPatch().isDirty = true;
            }
        }

        if (storage.getPatch().param_ptr[index]->affect_other_parameters)
        {
            storage.getPatch().update_controls();
            need_refresh = true;
        }

        switch (storage.getPatch().param_ptr[index]->ctrltype)
        {
        case ct_scenemode:
            release_if_latched[0] = true;
            release_if_latched[1] = true;
            release_anyway[0] = false;
            release_anyway[1] = false;
            break;
        case ct_polymode:
            if ((oldval.i == pm_latch) && (storage.getPatch().param_ptr[index]->val.i != pm_latch))
            {
                int s = storage.getPatch().param_ptr[index]->scene - 1;
                release_if_latched[s & 1] = true;
                release_anyway[s & 1] = true;
                if (!voices[s].empty())
                {
                    // The release if latched initiates a release scene
                    // which kills all voices but doesn't clear up the
                    // keyboard state. So
                    for (auto &v : voices[s])
                    {
                        const auto &st = v->state;
                        channelState[st.channel].keyState[st.key].keystate = 0;
                    }
                }
            }
            break;
        case ct_filtertype:
            switch_toggled_queued = true;
            {
                // Oh my goodness this is deeply sketchy code to not document! It assumes that
                // subtype is right after type in ID space without documenting it. So now
                // documented!
                auto typep = (storage.getPatch().param_ptr[index]);
                auto subtypep = storage.getPatch().param_ptr[index + 1];

                /* Used to be an lpmoog -> 3 type thing in here but that moved to the SurgeStorage
                 * ctor */
                subtypep->val.i =
                    storage.subtypeMemory[typep->scene - 1][typep->ctrlgroup_entry][typep->val.i];
            }
            refresh_editor = true;
            break;
        case ct_osctype:
        {
            int s = storage.getPatch().param_ptr[index]->scene - 1;

            if (storage.getPatch().param_ptr[index]->val.i != oldval.i)
            {
                /*
                 ** Wish there was a better way to figure out my osc but this works
                 */
                for (auto oi = 0; s >= 0 && s <= 1 && oi < n_oscs; oi++)
                {
                    if (storage.getPatch().scene[s].osc[oi].type.id ==
                        storage.getPatch().param_ptr[index]->id)
                    {
                        storage.getPatch().scene[s].osc[oi].queue_type =
                            storage.getPatch().param_ptr[index]->val.i;
                    }
                }
            }
            /*
             * Since we are setting a Queue, we don't need to toggle controls
             */
            switch_toggled_queued = false;
            need_refresh = true;
            refresh_editor = true;
            break;
        }
        case ct_wstype:
        case ct_bool_mute:
        case ct_fbconfig:
            switch_toggled_queued = true;
            break;
        case ct_filtersubtype:
            // See above: We know the filter type for this subtype is at index - 1. Cap max to be
            // the fut-subtype
            {
                auto subp = storage.getPatch().param_ptr[index];
                auto filterType = storage.getPatch().param_ptr[index - 1]->val.i;
                auto maxIVal = sst::filters::fut_subcount[filterType];
                if (maxIVal == 0)
                    subp->val.i = 0;
                else
                    subp->val.i = std::min(maxIVal - 1, subp->val.i);
                storage.subtypeMemory[subp->scene - 1][subp->ctrlgroup_entry][filterType] =
                    subp->val.i;

                switch_toggled_queued = true;
                break;
            }
        case ct_fxtype:
        {
            // OK so this would work you would think but remember that FXSync is primary not the
            // patch so
            auto p = storage.getPatch().param_ptr[index];
            if (p->val.i != oldval.i)
            {
                int cge = p->ctrlgroup_entry;

                fxsync[cge].type.val.i = p->val.i;

                // so funnily we want to set the value *back* so that loadFx picks up the change in
                // fxsync
                p->val.i = oldval.i;
                Effect *t_fx = spawn_effect(fxsync[cge].type.val.i, &storage, &fxsync[cge], 0);
                if (t_fx)
                {
                    t_fx->init_ctrltypes();
                    t_fx->init_default_values();
                    delete t_fx;
                }

                switch_toggled_queued = true;
                load_fx_needed = true;
                fx_reload[cge] = true;
            }
            break;
        }
        case ct_bool_relative_switch:
        {
            int s = storage.getPatch().param_ptr[index]->scene - 1;
            bool down = storage.getPatch().param_ptr[index]->val.b;
            float polarity = down ? -1.f : 1.f;

            if (oldval.b == down)
                polarity = 0.f;

            if (s >= 0)
            {
                storage.getPatch().scene[s].filterunit[1].cutoff.val.f +=
                    polarity * storage.getPatch().scene[s].filterunit[0].cutoff.val.f;
                storage.getPatch().scene[s].filterunit[1].envmod.val.f +=
                    polarity * storage.getPatch().scene[s].filterunit[0].envmod.val.f;
                storage.getPatch().scene[s].filterunit[1].keytrack.val.f +=
                    polarity * storage.getPatch().scene[s].filterunit[0].keytrack.val.f;
            }

            if (down)
            {
                storage.getPatch().scene[s].filterunit[1].cutoff.set_type(ct_freq_mod);
                storage.getPatch().scene[s].filterunit[1].cutoff.set_name("Offset");
            }
            else
            {
                storage.getPatch().scene[s].filterunit[1].cutoff.set_type(
                    ct_freq_audible_with_tunability);
                storage.getPatch().scene[s].filterunit[1].cutoff.set_name("Cutoff");
            }
            need_refresh = true;
        }
        break;
        case ct_envmode:
            refresh_editor = true;
            need_refresh = true; // See github issue #160
            break;
        case ct_bool_link_switch:
            need_refresh = true;
            break;
        case ct_bool_solo:
            // pre-1.7.2 unique (single) solo
            /*
            if (storage.getPatch().param_ptr[index]->val.b)
            {
               int s = storage.getPatch().param_ptr[index]->scene - 1;
               if (s >= 0)
               {
                  storage.getPatch().scene[s].solo_o1.val.b = false;
                  storage.getPatch().scene[s].solo_o2.val.b = false;
                  storage.getPatch().scene[s].solo_o3.val.b = false;
                  storage.getPatch().scene[s].solo_ring_12.val.b = false;
                  storage.getPatch().scene[s].solo_ring_23.val.b = false;
                  storage.getPatch().scene[s].solo_noise.val.b = false;
                  storage.getPatch().param_ptr[index]->val.b = true;
               }
            }
            */
            switch_toggled_queued = true;
            need_refresh = true;
            break;
        };
    }

    if (external && !need_refresh)
    {
        queueForRefresh(index);
    }
    return need_refresh;
}

void SurgeSynthesizer::queueForRefresh(int param_index)
{
    bool got = false;
    for (int i = 0; i < 8; i++)
    {
        if (refresh_parameter_queue[i] < 0 || refresh_parameter_queue[i] == param_index)
        {
            refresh_parameter_queue[i] = param_index;
            got = true;
            break;
        }
    }
    if (!got)
        refresh_overflow = true;
}

void SurgeSynthesizer::switch_toggled()
{
    for (int s = 0; s < n_scenes; s++)
    {
        list<SurgeVoice *>::iterator iter;
        for (iter = voices[s].begin(); iter != voices[s].end(); iter++)
        {
            SurgeVoice *v = *iter;
            v->switch_toggled();
        }
    }
}

bool SurgeSynthesizer::loadFx(bool initp, bool force_reload_all)
{
    load_fx_needed = false;
    bool localSendFX[n_fx_slots];
    for (int s = 0; s < n_fx_slots; s++)
    {
        localSendFX[s] = false;
        bool something_changed = false;
        if ((fxsync[s].type.val.i != storage.getPatch().fx[s].type.val.i) || force_reload_all ||
            fx_reload[s])
        {
            localSendFX[s] = true;
            storage.getPatch().isDirty = true;
            fx_reload[s] = false;

            std::lock_guard<std::mutex> g(fxSpawnMutex);

            fx[s].reset();
            /*if (!force_reload_all)*/ storage.getPatch().fx[s].type.val.i = fxsync[s].type.val.i;
            // else fxsync[s].type.val.i = storage.getPatch().fx[s].type.val.i;

            for (int j = 0; j < n_fx_params; j++)
            {
                storage.getPatch().fx[s].p[j].set_type(ct_none);
                std::string n = "Param ";
                n += std::to_string(j + 1);
                storage.getPatch().fx[s].p[j].set_name(n.c_str());
                storage.getPatch().fx[s].p[j].val.i = 0;
                storage.getPatch().globaldata[storage.getPatch().fx[s].p[j].id].i = 0;
            }

            if (/*!force_reload_all && */ storage.getPatch().fx[s].type.val.i)
            {
                std::copy(std::begin(fxsync[s].p), std::end(fxsync[s].p),
                          std::begin(storage.getPatch().fx[s].p));
            }

            fx[s].reset(spawn_effect(storage.getPatch().fx[s].type.val.i, &storage,
                                     &storage.getPatch().fx[s], storage.getPatch().globaldata));
            if (fx[s])
            {
                fx[s]->init_ctrltypes();
                if (initp)
                {
                    fx[s]->init_default_values();
                }
                else
                {
                    for (int j = 0; j < n_fx_params; j++)
                    {
                        auto p = &(storage.getPatch().fx[s].p[j]);
                        /*
                         * Alright well what the heck is this. "I can remove this" you may be
                         * thinking? Well - set_extend_range sets up the min and max for a value in
                         * some cases, and when unstreaming at this point, it is totally unclear
                         * whether it has been called correctly (and in many cases like move and
                         * load when I come out as a none but transmogrify to the right type above
                         * it hasn't) so we just set our extended status back onto ourselves and
                         * then those side effects which didn't happen through the init path are
                         * registered here and we can safely check against min and max values
                         */
                        p->set_extend_range(p->extend_range);

                        if (p->ctrltype != ct_none)
                        {
                            if (p->valtype == vt_float)
                            {
                                if (p->val.f < p->val_min.f)
                                {
                                    p->val.f = p->val_min.f;
                                }
                                if (p->val.f > p->val_max.f)
                                {
                                    p->val.f = p->val_max.f;
                                }
                            }
                            else if (p->valtype == vt_int)
                            {
                                if (p->val.i < p->val_min.i)
                                {
                                    p->val.i = p->val_min.i;
                                }
                                if (p->val.i > p->val_max.i)
                                {
                                    p->val.i = p->val_max.i;
                                }
                            }
                        }
                    }
                }
                /*for(int j=0; j<n_fx_params; j++)
                {
                    storage.getPatch().globaldata[storage.getPatch().fx[s].p[j].id].f =
                    storage.getPatch().fx[s].p[j].val.f;
                }*/

                fx[s]->init();

                /*
                ** Clear modulation onto FX otherwise it hangs around from old ones, often with
                ** disastrously bad meaning. #2036. But only do this if it is a one FX change
                ** (not a patch load)
                */
                if (!force_reload_all)
                {
                    for (int j = 0; j < n_fx_params; j++)
                    {
                        auto p = &(storage.getPatch().fx[s].p[j]);
                        for (int ms = 1; ms < n_modsources; ms++)
                        {
                            for (int sc = 0; sc < n_scenes; ++sc)
                            {
                                auto mi = getModulationIndicesBetween(p->id, (modsources)ms, sc);
                                for (auto m : mi)
                                {
                                    clearModulation(p->id, (modsources)ms, sc, m, true);
                                }
                            }
                        }
                    }
                    if (fx_reload_mod[s])
                    {
                        for (auto &t : fxmodsync[s])
                        {
                            setModDepth01(storage.getPatch().fx[s].p[t.whichForReal].id,
                                          (modsources)t.source_id, t.source_scene, t.source_index,
                                          t.depth);
                            muteModulation(storage.getPatch().fx[s].p[t.whichForReal].id,
                                           (modsources)t.source_id, t.source_scene, t.source_index,
                                           t.muted);
                        }
                        fxmodsync[s].clear();
                        fx_reload_mod[s] = false;
                    }
                }
            }
            else
            {
                // We have re-loaded to NULL; so we want to clear modulation that points at us
                // no matter what
                for (int j = 0; j < n_fx_params; j++)
                {
                    auto p = &(storage.getPatch().fx[s].p[j]);
                    for (int ms = 1; ms < n_modsources; ms++)
                    {
                        for (int sc = 0; sc < n_scenes; sc++)
                        {
                            auto mi = getModulationIndicesBetween(p->id, (modsources)ms, sc);
                            for (auto m : mi)
                            {
                                clearModulation(p->id, (modsources)ms, sc, m, true);
                            }
                        }
                    }
                }
            }

            something_changed = true;
            refresh_editor = true;
        }
        else if (fx_reload[s])
        {
            // This branch will happen when we change a preset for an FX; or when we turn an fx to
            // OFF
            storage.getPatch().isDirty = true;
            if (storage.getPatch().fx[s].type.val.i != fxt_off)
                std::copy(std::begin(fxsync[s].p), std::end(fxsync[s].p),
                          std::begin(storage.getPatch().fx[s].p));
            if (fx[s])
            {
                std::lock_guard<std::mutex> g(fxSpawnMutex);
                fx[s]->suspend();
                fx[s]->init();
            }
            fx_reload[s] = false;
            fx_reload_mod[s] = false;
            refresh_editor = true;
            something_changed = true;
        }

        if (fx[s] && something_changed)
        {
            fx[s]->updateAfterReload();
        }
    }

    if (!force_reload_all)
    {
        for (int s = 0; s < n_fx_slots; ++s)
        {
            resendFXParam[s] = localSendFX[s];
        }
    }

    // if (something_changed) storage.getPatch().update_controls(false);
    return true;
}

bool SurgeSynthesizer::loadOscalgos()
{
    bool algosChanged{false};
    bool localResendOscParams[n_scenes][n_oscs];
    for (int s = 0; s < n_scenes; s++)
    {
        for (int i = 0; i < n_oscs; i++)
        {
            localResendOscParams[s][i] = false;
            auto &osc_st = storage.getPatch().scene[s].osc[i];
            if (osc_st.queue_type > -1)
            {
                algosChanged = true;
                // clear assigned modulation, and echo to OSC if we change osc type, see issue #2224
                if (osc_st.queue_type != osc_st.type.val.i)
                {
                    clear_osc_modulation(s, i);
                }

                // Notify audio thread param change listeners (OSC, e.g.)
                // (which run on juce messenger thread)
                std::stringstream sb;
                sb << "/param/" << (char)('a' + s) << "/osc/" << i + 1 << "/type";
                auto new_type = osc_st.queue_type;
                for (const auto &it : audioThreadParamListeners)
                    (it.second)(sb.str(), new_type, osc_type_names[new_type]);

                osc_st.type.val.i = osc_st.queue_type;
                storage.getPatch().update_controls(false, &osc_st);
                osc_st.queue_type = -1;
                switch_toggled_queued = true;
                refresh_editor = true;
                localResendOscParams[s][i] = true;
            }

            TiXmlElement *e = (TiXmlElement *)osc_st.queue_xmldata;
            if (e)
            {
                storage.getPatch().isDirty = true;

                for (int k = 0; k < n_osc_params; k++)
                {
                    std::string oname = osc_st.p[k].oscName;
                    std::string sx = osc_st.p[k].get_name();

                    double d;
                    int j;
                    std::string lbl;

                    lbl = fmt::format("p{:d}", k);

                    if (osc_st.p[k].valtype == vt_float)
                    {
                        if (e->QueryDoubleAttribute(lbl.c_str(), &d) == TIXML_SUCCESS)
                        {
                            osc_st.p[k].val.f = (float)d;
                            for (const auto &it : audioThreadParamListeners)
                                (it.second)(oname, d, sx);
                        }
                    }
                    else
                    {
                        if (e->QueryIntAttribute(lbl.c_str(), &j) == TIXML_SUCCESS)
                        {
                            osc_st.p[k].val.i = j;
                            for (const auto &it : audioThreadParamListeners)
                                (it.second)(oname, j, sx);
                        }
                    }

                    lbl = fmt::format("p{:d}_deform_type", k);

                    if (e->QueryIntAttribute(lbl.c_str(), &j) == TIXML_SUCCESS)
                    {
                        osc_st.p[k].deform_type = j;
                        for (const auto &it : audioThreadParamListeners)
                            (it.second)(oname, j, sx);
                    }

                    lbl = fmt::format("p{:d}_extend_range", k);

                    if (e->QueryIntAttribute(lbl.c_str(), &j) == TIXML_SUCCESS)
                    {
                        osc_st.p[k].set_extend_range(j);
                        for (const auto &it : audioThreadParamListeners)
                            (it.second)(oname, j, sx);
                    }
                }

                int rt;
                if (e->QueryIntAttribute("retrigger", &rt) == TIXML_SUCCESS)
                {
                    osc_st.retrigger.val.b = rt;
                    std::stringstream sb;
                    sb << "/param/" << (char)('a' + s) << "/osc/" << i + 1 << "/retrigger";
                    std::string sx = rt > 0 ? "On" : "Off";
                    for (const auto &it : audioThreadParamListeners)
                        (it.second)(sb.str(), rt, sx);
                }

                /*
                 * Some oscillator types can change display when you change values
                 */
                if (osc_st.type.val.i == ot_modern)
                {
                    refresh_editor = true;
                }
                osc_st.queue_xmldata = 0;
            }
        }
    }

    if (algosChanged)
    {
        storage.memoryPools->resetOscillatorPools(&storage);
        for (int s = 0; s < n_scenes; ++s)
        {
            for (int o = 0; o < n_oscs; ++o)
            {
                resendOscParam[s][o] = localResendOscParams[s][o];
            }
        }
    }
    return true;
}

bool SurgeSynthesizer::isModulatorDistinctPerScene(modsources modsource) const
{
    if (modsource >= ms_lfo1 && modsource <= ms_slfo6)
        return true;

    if (modsource == ms_ampeg || modsource == ms_filtereg)
        return true;

    if (modsource == ms_lowest_key || modsource == ms_highest_key || modsource == ms_latest_key)
        return true;

    return false;
}

bool SurgeSynthesizer::isValidModulation(long ptag, modsources modsource) const
{
    if (!modsource)
        return false;
    if (!(ptag < storage.getPatch().param_ptr.size()))
        return false;

    Parameter *p = storage.getPatch().param_ptr[ptag];

    if (!p->modulateable)
        return false;
    if (p->valtype != ((valtypes)vt_float))
        return false;
    if (!p->per_voice_processing && !canModulateMonophonicTarget(modsource))
        return false;

    for (int sc = 0; sc < n_scenes; sc++)
    {
        if ((modsource == ms_keytrack) && (p == &storage.getPatch().scene[sc].pitch))
            return false;
    }
    if ((p->ctrlgroup == cg_LFO) && (p->ctrlgroup_entry == modsource))
        return false;
    if ((p->ctrlgroup == cg_LFO) && (p->ctrlgroup_entry >= ms_slfo1) && (!isScenelevel(modsource)))
        return false;
    if ((p->ctrlgroup == cg_ENV) && !canModulateModulators(modsource))
        return false;
    return true;
}

std::vector<int> SurgeSynthesizer::getModulationIndicesBetween(long ptag, modsources modsource,
                                                               int modsourceScene) const
{
    std::vector<int> res;

    if (!isValidModulation(ptag, modsource))
        return res;

    int scene = storage.getPatch().param_ptr[ptag]->scene;
    vector<ModulationRouting> *modlist = nullptr;

    if (!scene)
    {
        modlist = &storage.getPatch().modulation_global;
    }
    else
    {
        if (isScenelevel(modsource))
            modlist = &storage.getPatch().scene[scene - 1].modulation_scene;
        else
            modlist = &storage.getPatch().scene[scene - 1].modulation_voice;
    }

    int id = storage.getPatch().param_ptr[ptag]->param_id_in_scene;
    if (!scene)
        id = ptag;
    int n = modlist->size();

    for (int i = 0; i < n; i++)
    {
        if ((modlist->at(i).destination_id == id) && (modlist->at(i).source_id == modsource) &&
            (scene || modlist->at(i).source_scene == modsourceScene))
        {
            res.push_back(modlist->at(i).source_index);
        }
    }

    return res;
}

ModulationRouting *SurgeSynthesizer::getModRouting(long ptag, modsources modsource,
                                                   int modsourceScene, int index) const
{
    if (!isValidModulation(ptag, modsource))
        return nullptr;

    int scene = storage.getPatch().param_ptr[ptag]->scene;
    vector<ModulationRouting> *modlist = nullptr;

    if (!scene)
    {
        modlist = &storage.getPatch().modulation_global;
    }
    else
    {
        if (isScenelevel(modsource))
            modlist = &storage.getPatch().scene[scene - 1].modulation_scene;
        else
            modlist = &storage.getPatch().scene[scene - 1].modulation_voice;
    }

    int id = storage.getPatch().param_ptr[ptag]->param_id_in_scene;
    if (!scene)
        id = ptag;
    int n = modlist->size();

    for (int i = 0; i < n; i++)
    {
        if ((modlist->at(i).destination_id == id) && (modlist->at(i).source_id == modsource) &&
            (modlist->at(i).source_index == index) &&
            (scene || modlist->at(i).source_scene == modsourceScene))
        {
            return &modlist->at(i);
        }
    }

    return nullptr;
}

float SurgeSynthesizer::getModDepth(long ptag, modsources modsource, int modsourceScene,
                                    int index) const
{
    auto *r = getModRouting(ptag, modsource, modsourceScene, index);
    float d = 0.f;
    if (r)
        d = r->depth;
    Parameter *p = storage.getPatch().param_ptr.at(ptag);
    if (p && p->extend_range)
        d = p->get_extended(d);
    return d;
}

bool SurgeSynthesizer::isActiveModulation(long ptag, modsources modsource, int modsourceScene,
                                          int index) const
{
    // if(!isValidModulation(ptag,modsource)) return false;
    if (getModRouting(ptag, modsource, modsourceScene, index))
        return true;
    return false;
}

bool SurgeSynthesizer::isAnyActiveModulation(long ptag, modsources modsource,
                                             int modsourceScene) const
{
    return !getModulationIndicesBetween(ptag, modsource, modsourceScene).empty();
}

bool SurgeSynthesizer::isBipolarModulation(modsources tms) const
{
    int scene_ms = storage.getPatch().scene_active.val.i;

    // You would think you could just do this nad ask for is_bipolar but remember the LFOs are made
    // at voice time so...
    if (tms >= ms_lfo1 && tms <= ms_slfo6)
    {
        bool isup =
            storage.getPatch().scene[scene_ms].lfo[tms - ms_lfo1].unipolar.val.i ||
            storage.getPatch().scene[scene_ms].lfo[tms - ms_lfo1].shape.val.i == lt_envelope;

        // For now
        return !isup;
    }
    if (tms >= ms_ctrl1 && tms <= ms_ctrl8)
    {
        // Macros can also be bipolar
        auto ms = storage.getPatch().scene[scene_ms].modsources[tms];
        if (ms)
            return ms->is_bipolar();
        else
            return false;
    }
    if (tms == ms_keytrack || tms == ms_lowest_key || tms == ms_highest_key ||
        tms == ms_latest_key || tms == ms_pitchbend || tms == ms_random_bipolar ||
        tms == ms_alternate_bipolar || tms == ms_timbre)
        return true;
    else
        return false;
}

bool SurgeSynthesizer::isModDestUsed(long ptag) const
{
    int scene_ms = storage.getPatch().scene_active.val.i;
    int scene_p = storage.getPatch().param_ptr[ptag]->scene;

    long md_id = storage.getPatch().param_ptr[ptag]->id;
    if (scene_p)
        md_id = storage.getPatch().param_ptr[ptag]->param_id_in_scene;

    for (int j = 0; j < 3; j++)
    {
        // if((scene && (j>0))||(!scene && (j==0)))
        if ((scene_p && (j > 0)) || (!scene_p && (j == 0)))
        {
            const vector<ModulationRouting> *modlist{nullptr};

            switch (j)
            {
            case 0:
                modlist = &storage.getPatch().modulation_global;
                break;
            case 1:
                modlist = &storage.getPatch().scene[scene_ms].modulation_scene;
                break;
            case 2:
                modlist = &storage.getPatch().scene[scene_ms].modulation_voice;
                break;
            }

            int n = modlist->size();
            for (int i = 0; i < n; i++)
            {
                if (modlist->at(i).destination_id == md_id)
                    return true;
            }
        }
    }
    return false;
}

void SurgeSynthesizer::updateUsedState()
{
    storage.modRoutingMutex.lock();

    // intended for GUI only
    for (int i = 0; i < n_modsources; i++)
        modsourceused[i] = false;

    int scene = storage.getPatch().scene_active.val.i;

    for (int j = 0; j < 3; j++)
    {
        vector<ModulationRouting> *modlist;

        switch (j)
        {
        case 0:
            modlist = &storage.getPatch().modulation_global;
            break;
        case 1:
            modlist = &storage.getPatch().scene[scene].modulation_scene;
            break;
        case 2:
            modlist = &storage.getPatch().scene[scene].modulation_voice;
            break;
        }

        int n = modlist->size();
        for (int i = 0; i < n; i++)
        {
            int id = modlist->at(i).source_id;
            if (!isModulatorDistinctPerScene((modsources)id) ||
                modlist->at(i).source_scene == scene)
                modsourceused[id] = true;
        }
    }

    storage.modRoutingMutex.unlock();
}

void SurgeSynthesizer::prepareModsourceDoProcess(int scenemask)
{
    for (int scene = 0; scene < n_scenes; scene++)
    {
        bool anyFormula{false};
        for (auto &lfs : storage.getPatch().scene[scene].lfo)
        {
            if (lfs.shape.val.i == lt_formula)
            {
                anyFormula = true;
            }
        }
        if ((1 << scene) & scenemask)
        {
            for (int i = 0; i < n_modsources; i++)
            {
                bool setTo = false;
                if (i >= ms_lfo1 && i <= ms_lfo6)
                {
                    auto lf = &(storage.getPatch().scene[scene].lfo[i - ms_lfo1]);
                    if (lf->shape.val.i == lt_stepseq)
                    {
                        auto ss = &(storage.getPatch().stepsequences[scene][i - ms_lfo1]);
                        setTo = (ss->trigmask != 0);
                    }
                }
                storage.getPatch().scene[scene].modsource_doprocess[i] = setTo;

                if (i == ms_pitchbend || i == ms_aftertouch || i == ms_modwheel || i == ms_breath ||
                    i == ms_expression || i == ms_sustain || i == ms_lowest_key ||
                    i == ms_highest_key || i == ms_latest_key)
                {
                    storage.getPatch().scene[scene].modsource_doprocess[i] |= anyFormula;
                }
            }

            for (int j = 0; j < 3; j++)
            {
                vector<ModulationRouting> *modlist;

                switch (j)
                {
                case 0:
                    modlist = &storage.getPatch().modulation_global;
                    break;
                case 1:
                    modlist = &storage.getPatch().scene[scene].modulation_scene;
                    break;
                case 2:
                    modlist = &storage.getPatch().scene[scene].modulation_voice;
                    break;
                }

                int n = modlist->size();
                for (int i = 0; i < n; i++)
                {
                    int id = modlist->at(i).source_id;
                    assert((id > 0) && (id < n_modsources));
                    storage.getPatch().scene[scene].modsource_doprocess[id] = true;
                }
            }
        }
    }
}

bool SurgeSynthesizer::isModsourceUsed(modsources modsource)
{
    updateUsedState();
    return modsourceused[modsource];
}

float SurgeSynthesizer::getModDepth01(long ptag, modsources modsource, int modsourceScene,
                                      int index) const
{
    if (!isValidModulation(ptag, modsource))
        return 0.0f;

    auto *r = getModRouting(ptag, modsource, modsourceScene, index);
    if (r)
        return storage.getPatch().param_ptr[ptag]->get_modulation_f01(r->depth);

    return storage.getPatch().param_ptr[ptag]->get_modulation_f01(0);
}

bool SurgeSynthesizer::isModulationMuted(long ptag, modsources modsource, int modsourceScene,
                                         int index) const
{
    if (!isValidModulation(ptag, modsource))
        return false;

    auto *r = getModRouting(ptag, modsource, modsourceScene, index);
    if (r)
        return r->muted;

    return false;
}

void SurgeSynthesizer::muteModulation(long ptag, modsources modsource, int modsourceScene,
                                      int index, bool mute)
{
    if (!isValidModulation(ptag, modsource))
        return;

    ModulationRouting *r = getModRouting(ptag, modsource, modsourceScene, index);
    if (r)
    {
        r->muted = mute;
        storage.getPatch().isDirty = true;

        for (auto l : modListeners)
            l->modMuted(ptag, modsource, modsourceScene, index, mute);
    }
}

void SurgeSynthesizer::applyParameterMonophonicModulation(Parameter *p, float depth)
{
    auto &pt = storage.getPatch();
    if (pt.paramModulationCount >= pt.maxMonophonicParamModulations)
    {
        // hmmm ... what to do here?
        return;
    }
    // This linear search will become, i think, quite tiresome at size
    for (int i = 0; i < pt.paramModulationCount; ++i)
    {
        if (pt.monophonicParamModulations[i].param_id == p->id)
        {
            pt.monophonicParamModulations[i].vt_type = (valtypes)p->valtype;
            switch (p->valtype)
            {
            case vt_float:
                pt.monophonicParamModulations[i].value = depth * (p->val_max.f - p->val_min.f);
                break;
            case vt_int:
                pt.monophonicParamModulations[i].value = depth * (p->val_max.i - p->val_min.i);
                pt.monophonicParamModulations[i].imin = p->val_min.i;
                pt.monophonicParamModulations[i].imax = p->val_max.i;

                break;
            case vt_bool:
                pt.monophonicParamModulations[i].value = depth;
                break;
            }
            return;
        }
    }

    assert(pt.paramModulationCount < pt.maxMonophonicParamModulations);
    pt.monophonicParamModulations[pt.paramModulationCount].param_id = p->id;
    pt.monophonicParamModulations[pt.paramModulationCount].vt_type = (valtypes)p->valtype;
    switch (p->valtype)
    {
    case vt_float:
        pt.monophonicParamModulations[pt.paramModulationCount].value =
            depth * (p->val_max.f - p->val_min.f);
        break;
    case vt_int:
        pt.monophonicParamModulations[pt.paramModulationCount].value =
            depth * (p->val_max.i - p->val_min.i);
        pt.monophonicParamModulations[pt.paramModulationCount].imin = p->val_min.i;
        pt.monophonicParamModulations[pt.paramModulationCount].imax = p->val_max.i;
        break;
    case vt_bool:
        pt.monophonicParamModulations[pt.paramModulationCount].value = depth;
        break;
    }
    pt.paramModulationCount++;
    return;
}

void SurgeSynthesizer::applyParameterPolyphonicModulation(Parameter *p, int32_t note_id,
                                                          int16_t key, int16_t channel, float depth)
{
    // in theory, a parameter without a scene will never get poly modulation applied
    if (p->scene == 0)
        return;

    /*
     * The CLAP specification says if a monophonic modulation is in effect, that the host needs
     * to stack it with a polyphonic modulation, not the plugin. So if a polyphonic modulation
     * starts on a parameter on a voice it includes any mono mod which may be untargeted. Since
     * surge stacks these separately, we need to back that modulation out of the voice application.
     */
    float underlyingMonoMod{0};
    auto &pt = storage.getPatch();
    // This linear search will become, i think, quite tiresome at size
    for (int i = 0; i < pt.paramModulationCount; ++i)
    {
        if (pt.monophonicParamModulations[i].param_id == p->id)
        {
            underlyingMonoMod = pt.monophonicParamModulations[i].value;
        }
    }

    for (auto v : voices[p->scene - 1])
    {
        if (v->matchesChannelKeyId(channel, key, note_id))
        {
            v->applyPolyphonicParamModulation(p, depth, underlyingMonoMod);
        }
    }
}

void SurgeSynthesizer::clear_osc_modulation(int scene, int entry)
{
    storage.modRoutingMutex.lock();
    vector<ModulationRouting>::iterator iter;

    int pid = storage.getPatch().scene[scene].osc[entry].p[0].param_id_in_scene;
    iter = storage.getPatch().scene[scene].modulation_scene.begin();
    while (iter != storage.getPatch().scene[scene].modulation_scene.end())
    {
        if ((iter->destination_id >= pid) && (iter->destination_id < (pid + n_osc_params)))
        {
            iter = storage.getPatch().scene[scene].modulation_scene.erase(iter);
        }
        else
            iter++;
    }
    iter = storage.getPatch().scene[scene].modulation_voice.begin();
    while (iter != storage.getPatch().scene[scene].modulation_voice.end())
    {
        if ((iter->destination_id >= pid) && (iter->destination_id < (pid + n_osc_params)))
        {
            iter = storage.getPatch().scene[scene].modulation_voice.erase(iter);
        }
        else
            iter++;
    }
    storage.modRoutingMutex.unlock();
}

bool SurgeSynthesizer::supportsIndexedModulator(int scene, modsources modsource) const
{
    if (modsource >= ms_lfo1 && modsource <= ms_slfo6)
    {
        return true;
        // auto lf = &(storage.getPatch().scene[scene].lfo[modsource - ms_lfo1]);
        // return lf->shape.val.i == lt_formula;
    }

    if (modsource == ms_random_bipolar || modsource == ms_random_unipolar)
    {
        return true;
    }
    return false;
}

int SurgeSynthesizer::getMaxModulationIndex(int scene, modsources modsource) const
{
    if (modsource >= ms_lfo1 && modsource <= ms_slfo6)
    {
        auto lf = &(storage.getPatch().scene[scene].lfo[modsource - ms_lfo1]);
        if (lf->shape.val.i == lt_formula)
        {
            return Surge::Formula::max_formula_outputs;
        }
        else
            return 3;
    }
    if (modsource == ms_random_bipolar)
        return 2;
    if (modsource == ms_random_unipolar)
        return 2;
    return 1;
}

void SurgeSynthesizer::clearModulation(long ptag, modsources modsource, int modsourceScene,
                                       int index, bool clearEvenIfInvalid)
{
    if (!isValidModulation(ptag, modsource) && !clearEvenIfInvalid)
    {
        return;
    }

    int scene = storage.getPatch().param_ptr[ptag]->scene;

    vector<ModulationRouting> *modlist;

    if (!scene)
    {
        modlist = &storage.getPatch().modulation_global;
    }
    else
    {
        if (isScenelevel(modsource))
        {
            modlist = &storage.getPatch().scene[scene - 1].modulation_scene;
        }
        else
        {
            modlist = &storage.getPatch().scene[scene - 1].modulation_voice;
        }
    }

    int id = storage.getPatch().param_ptr[ptag]->param_id_in_scene;

    if (!scene)
    {
        id = ptag;
    }

    int n = modlist->size();

    for (int i = 0; i < n; i++)
    {
        if ((modlist->at(i).destination_id == id) && (modlist->at(i).source_id == modsource) &&
            (modlist->at(i).source_index == index) &&
            (scene || modlist->at(i).source_scene == modsourceScene))
        {
            storage.modRoutingMutex.lock();
            modlist->erase(modlist->begin() + i);
            storage.modRoutingMutex.unlock();
            storage.getPatch().isDirty = true;

            for (auto l : modListeners)
            {
                l->modCleared(ptag, modsource, modsourceScene, index);
            }

            return;
        }
    }
}

bool SurgeSynthesizer::setModDepth01(long ptag, modsources modsource, int modsourceScene, int index,
                                     float val)
{
    if (!isValidModulation(ptag, modsource))
        return false;
    float value = storage.getPatch().param_ptr[ptag]->set_modulation_f01(val);
    int scene = storage.getPatch().param_ptr[ptag]->scene;
    storage.getPatch().isDirty = true;
    vector<ModulationRouting> *modlist;

    if (!scene)
    {
        modlist = &storage.getPatch().modulation_global;
    }
    else
    {
        if (isScenelevel(modsource))
            modlist = &storage.getPatch().scene[scene - 1].modulation_scene;
        else
            modlist = &storage.getPatch().scene[scene - 1].modulation_voice;
    }

    storage.modRoutingMutex.lock();

    int id = storage.getPatch().param_ptr[ptag]->param_id_in_scene;
    if (!scene)
        id = ptag;
    int n = modlist->size();
    int found_id = -1;
    for (int i = 0; i < n; i++)
    {
        if ((modlist->at(i).destination_id == id) && (modlist->at(i).source_id == modsource) &&
            (modlist->at(i).source_index == index) &&
            (scene || modlist->at(i).source_scene == modsourceScene))
        {
            found_id = i;
            break;
        }
    }

    if (value == 0)
    {
        if (found_id >= 0)
        {
            modlist->erase(modlist->begin() + found_id);
        }
    }
    else
    {
        if (found_id < 0)
        {
            ModulationRouting t;
            t.depth = value;
            t.source_id = modsource;
            t.destination_id = id;
            t.muted = false;
            t.source_index = index;
            t.source_scene = modsourceScene;
            modlist->push_back(t);
        }
        else
        {
            modlist->at(found_id).depth = value;
        }
    }
    storage.modRoutingMutex.unlock();

    for (auto l : modListeners)
        l->modSet(ptag, modsource, modsourceScene, index, val, found_id < 0);
    return true;
}

float SurgeSynthesizer::getMacroParameter01(long macroNum) const
{
    return storage.getPatch().scene[0].modsources[ms_ctrl1 + macroNum]->get_output01(0);
}

float SurgeSynthesizer::getMacroParameterTarget01(long macroNum) const
{
    return ((ControllerModulationSource *)storage.getPatch().scene[0].modsources[ms_ctrl1 +
                                                                                 macroNum])
        ->target[0];
}

void SurgeSynthesizer::setMacroParameter01(long macroNum, float val)
{
    storage.getPatch().isDirty = true;
    ((ControllerModulationSource *)storage.getPatch().scene[0].modsources[ms_ctrl1 + macroNum])
        ->set_target01(val, true);
}

void SurgeSynthesizer::applyMacroMonophonicModulation(long macroNum, float val)
{
    ((MacroModulationSource *)storage.getPatch().scene[0].modsources[ms_ctrl1 + macroNum])
        ->setModulationDepth(val);
}

float SurgeSynthesizer::getParameter01(long index) const
{
    if (index >= 0 && index < storage.getPatch().param_ptr.size())
        return storage.getPatch().param_ptr[index]->get_value_f01();
    return 0.f;
}

void SurgeSynthesizer::getParameterDisplay(long index, char *text) const
{
    if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
    {
        storage.getPatch().param_ptr[index]->get_display(text);
    }
    else
    {
        snprintf(text, TXT_SIZE, "-");
    }
}

void SurgeSynthesizer::getParameterDisplayAlt(long index, char *text) const
{
    if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
    {
        storage.getPatch().param_ptr[index]->get_display_alt(text);
    }
    else
    {
        text[0] = 0;
    }
}

void SurgeSynthesizer::getParameterDisplay(long index, char *text, float x) const
{
    if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
    {
        storage.getPatch().param_ptr[index]->get_display(text, true, x);
    }
    else
        snprintf(text, TXT_SIZE, "-");
}

void SurgeSynthesizer::getParameterName(long index, char *text) const
{
    if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
    {
        int scn = storage.getPatch().param_ptr[index]->scene;
        // TODO: FIX SCENE ASSUMPTION
        string sn[3] = {"", "A ", "B "};

        snprintf(text, TXT_SIZE, "%s%s", sn[scn].c_str(),
                 storage.getPatch().param_ptr[index]->get_full_name());
    }
    else
        snprintf(text, TXT_SIZE, "-");
}

void SurgeSynthesizer::getParameterNameExtendedByFXGroup(long index, char *text) const
{
    if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
    {
        auto par = storage.getPatch().param_ptr[index];

        if (par->ctrlgroup != cg_FX)
        {
            getParameterName(index, text);
            return;
        }

        std::string fxGroupName{};
        auto p0 = &storage.getPatch().fx[par->ctrlgroup_entry].p[0];
        auto df = par - p0;
        if (df >= 0 && df < n_fx_params)
        {
            auto &fxi = fx[par->ctrlgroup_entry];
            if (fxi)
            {
                auto gi = fxi->groupIndexForParamIndex(df);
                if (gi >= 0)
                {
                    auto gn = fxi->group_label(gi);
                    if (gn)
                    {
                        fxGroupName = gn;
                        fxGroupName = fxGroupName + " - ";
                    }
                }
            }
        }
        snprintf(text, TXT_SIZE, "%s %s%s", fxslot_shortnames[par->ctrlgroup_entry],
                 fxGroupName.c_str(), par->get_name());
    }
    else
        snprintf(text, TXT_SIZE, "-");
}

void SurgeSynthesizer::getParameterAccessibleName(long index, char *text) const
{
    if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
    {
        int scn = storage.getPatch().param_ptr[index]->scene;
        // TODO: FIX SCENE ASSUMPTION
        string sn[3] = {"", "Scene A ", "Scene B "};

        snprintf(text, TXT_SIZE, "%s%s", sn[scn].c_str(),
                 storage.getPatch().param_ptr[index]->get_full_name());
    }
    else
        snprintf(text, TXT_SIZE, "-");
}

void SurgeSynthesizer::getParameterMeta(long index, parametermeta &pm) const
{
    if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
    {
        pm.flags = storage.getPatch().param_ptr[index]->ctrlstyle;
        pm.fmin = 0.f; // storage.getPatch().param_ptr[index]->val_min.f;
        pm.fmax = 1.f; // storage.getPatch().param_ptr[index]->val_max.f;
        pm.fdefault =
            storage.getPatch()
                .param_ptr[index]
                ->get_default_value_f01(); // storage.getPatch().param_ptr[index]->val_default.f;
        pm.hide = (pm.flags & Surge::ParamConfig::kHide) != 0;
        pm.meta = (pm.flags & Surge::ParamConfig::kMeta) != 0;
        pm.expert = !(pm.flags & Surge::ParamConfig::kEasy);
        pm.clump = 2;
        if (storage.getPatch().param_ptr[index]->scene)
        {
            pm.clump += storage.getPatch().param_ptr[index]->ctrlgroup +
                        (storage.getPatch().param_ptr[index]->scene - 1) * 6;
            if (storage.getPatch().param_ptr[index]->ctrlgroup == 0)
                pm.clump++;
        }
    }
}

float SurgeSynthesizer::getParameter(long index) const
{
    if (index >= 0 && index < storage.getPatch().param_ptr.size())
        return storage.getPatch().param_ptr[index]->get_value_f01();
    return 0.f;
}

float SurgeSynthesizer::normalizedToValue(long index, float value) const
{
    if (index >= 0 && index < storage.getPatch().param_ptr.size())
        return storage.getPatch().param_ptr[index]->normalized_to_value(value);
    return 0.f;
}

float SurgeSynthesizer::valueToNormalized(long index, float value) const
{
    if (index >= 0 && index < storage.getPatch().param_ptr.size())
        return storage.getPatch().param_ptr[index]->value_to_normalized(value);
    return 0.f;
}

bool SurgeSynthesizer::stringToNormalizedValue(const ID &index, std::string s, float &outval) const
{
    int id = index.getSynthSideId();
    if (id < 0 || id >= storage.getPatch().param_ptr.size())
        return false;

    auto p = storage.getPatch().param_ptr[id];
    if (p->valtype != vt_float)
        return false;

    pdata vt = p->val;
    std::string errMsg;
    auto res = p->set_value_from_string_onto(s, vt, errMsg);

    if (res)
    {
        outval = (vt.f - p->val_min.f) / (p->val_max.f - p->val_min.f);
        return true;
    }

    return false;
}

void loadPatchInBackgroundThread(SurgeSynthesizer *sy)
{
    fs::path ppath;
    int patchid = -1;
    bool had_patchid_file = false;

    SurgeSynthesizer *synth = (SurgeSynthesizer *)sy;
    std::lock_guard<std::mutex> mg(synth->patchLoadSpawnMutex);
    if (synth->patchid_queue >= 0)
    {
        patchid = synth->patchid_queue;
        synth->patchid_queue = -1;
        synth->stopSound();
        synth->loadPatch(patchid);
    }
    if (synth->has_patchid_file)
    {
        ppath = string_to_path(synth->patchid_file);
        synth->has_patchid_file = false;
        had_patchid_file = true;
        synth->stopSound();

        int ptid = -1, ct = 0;
        for (const auto &pti : synth->storage.patch_list)
        {
            if (path_to_string(pti.path) == synth->patchid_file)
            {
                ptid = ct;
            }
            ct++;
        }

        if (ptid >= 0)
        {
            synth->loadPatch(ptid);
        }
        else
        {
            synth->loadPatchByPath(synth->patchid_file, -1, path_to_string(ppath).c_str());
        }
    }

    synth->storage.getPatch().isDirty = false;
    synth->patchChanged = true;
    synth->halt_engine = false;

    // Notify the 'patch loaded' listener(s)
    // Note that this is not an if/else for good reason: both cases may occur simultaneously
    if (patchid >= 0)
    {
        Patch p = synth->storage.patch_list[synth->patchid];
        synth->storage.lastLoadedPatch = p.path;
        for (auto &it : synth->patchLoadedListeners)
            (it.second)(p.path);
    }
    if (had_patchid_file)
    {
        synth->storage.lastLoadedPatch = ppath;
        for (auto &it : synth->patchLoadedListeners)
            (it.second)(ppath);
    }

    // Now we want to null out the patchLoadThread since everything is done
    auto myThread = std::move(synth->patchLoadThread);
    myThread->detach();

    return;
}

void SurgeSynthesizer::processAudioThreadOpsWhenAudioEngineUnavailable(bool dangerMode)
{
    if (!audio_processing_active || dangerMode)
    {
        processEnqueuedPatchIfNeeded();

        auto lg = std::lock_guard<std::mutex>(patchLoadSpawnMutex);

        // if the audio processing is inactive, patchloading should occur anyway
        if (patchid_queue >= 0)
        {
            loadPatch(patchid_queue);
            Patch p = storage.patch_list[patchid_queue];
            storage.lastLoadedPatch = p.path;
            patchid_queue = -1;
        }

        if (has_patchid_file)
        {
            auto p(string_to_path(patchid_file));
            auto s = path_to_string(p.stem());
            has_patchid_file = false;

            int ptid = -1, ct = 0;
            for (const auto &pti : storage.patch_list)
            {
                if (path_to_string(pti.path) == patchid_file)
                {
                    ptid = ct;
                }
                ct++;
            }
            if (ptid >= 0)
            {
                loadPatch(ptid);
                Patch patch = storage.patch_list[ptid];
                storage.lastLoadedPatch = patch.path;
            }
            else
            {
                loadPatchByPath(patchid_file, -1, s.c_str());
                storage.lastLoadedPatch = p;
            }
            patchid_file[0] = 0;
        }

        if (load_fx_needed)
            loadFx(false, false);

        loadOscalgos();

        storage.perform_queued_wtloads();
    }
}

void SurgeSynthesizer::resetStateFromTimeData()
{
    if (time_data.timeSigNumerator < 1)
        time_data.timeSigNumerator = 4;
    if (time_data.timeSigDenominator < 1)
        time_data.timeSigDenominator = 4;
    storage.songpos = time_data.ppqPos;
    if (time_data.tempo > 0)
    {
        storage.temposyncratio = time_data.tempo / 120.f;
        storage.temposyncratio_inv = 1.f / storage.temposyncratio;
    }
    else
    {
        storage.temposyncratio = 1.f;
        storage.temposyncratio_inv = 1.f;
    }
}

void SurgeSynthesizer::enqueueFXOff(int whichFX)
{
    // this can come from the UI thread. I don't think we need the spawn mutex but we might
    std::lock_guard<std::mutex> lg(fxSpawnMutex);
    fxsync[whichFX].type.val.i = fxt_off;
    load_fx_needed = true;
}

void SurgeSynthesizer::processControl()
{
    processEnqueuedPatchIfNeeded();

    storage.perform_queued_wtloads();
    int sm = storage.getPatch().scenemode.val.i;
    // TODO: FIX SCENE ASSUMPTION
    bool playA = (sm == sm_split) || (sm == sm_dual) || (sm == sm_chsplit) ||
                 (storage.getPatch().scene_active.val.i == 0);
    bool playB = (sm == sm_split) || (sm == sm_dual) || (sm == sm_chsplit) ||
                 (storage.getPatch().scene_active.val.i == 1);

    storage.songpos = time_data.ppqPos;
    storage.temposyncratio = time_data.tempo / 120.f;
    storage.temposyncratio_inv = 1.f / storage.temposyncratio;

    // TODO: FIX SCENE ASSUMPTION
    if (release_if_latched[0])
    {
        if (!playA || release_anyway[0])
            releaseScene(0);
        release_if_latched[0] = false;
        release_anyway[0] = false;
    }
    if (release_if_latched[1])
    {
        if (!playB || release_anyway[1])
            releaseScene(1);
        release_if_latched[1] = false;
        release_anyway[1] = false;
    }

    // interpolate MIDI controllers
    for (int i = 0; i < num_controlinterpolators; i++)
    {
        if (mControlInterpolatorUsed[i])
        {
            ControllerModulationSource *mc = &mControlInterpolator[i];
            bool cont = mc->process_block_until_close(0.001f);
            int id = mc->id;
            storage.getPatch().param_ptr[id]->set_value_f01(mc->get_output(0));
            if (!cont)
            {
                mControlInterpolatorUsed[i] = false;
            }
        }
    }

    // Update keys if we are bound
    prepareModsourceDoProcess((playA ? 1 : 0) | (playB ? 2 : 0));

    for (int sc = 0; sc < n_scenes; ++sc)
    {
        if (storage.getPatch().scene[sc].modsource_doprocess[ms_highest_key] ||
            storage.getPatch().scene[sc].modsource_doprocess[ms_lowest_key] ||
            storage.getPatch().scene[sc].modsource_doprocess[ms_latest_key])
        {
            updateHighLowKeys(sc);
        }
    }

    storage.getPatch().copy_globaldata(
        storage.getPatch()
            .globaldata); // Drains a great deal of CPU while in Debug mode.. optimize?

    // TODO: FIX SCENE ASSUMPTION
    if (playA)
        storage.getPatch().copy_scenedata(storage.getPatch().scenedata[0],
                                          storage.getPatch().scenedataOrig[0], 0); // -""-
    if (playB)
        storage.getPatch().copy_scenedata(storage.getPatch().scenedata[1],
                                          storage.getPatch().scenedataOrig[1], 1);

    // TODO: FIX SCENE ASSUMPTION.
    // Prior to 1.1 we could play before or after copying modulation data but as we
    // introduce int mods, we need to make sure the scenedata and so on is set up before
    // we latch
    if (playA && (storage.getPatch().scene[0].polymode.val.i == pm_latch) && voices[0].empty())
        playNote(1, 60, 100, 0, -1, 0);
    if (playB && (storage.getPatch().scene[1].polymode.val.i == pm_latch) && voices[1].empty())
        playNote(2, 60, 100, 0, -1, 1);

    for (int s = 0; s < n_scenes; s++)
    {
        if (((s == 0) && playA) || ((s == 1) && playB))
        {
            if (storage.getPatch().scene[s].modsource_doprocess[ms_modwheel])
                storage.getPatch().scene[s].modsources[ms_modwheel]->process_block();
            if (storage.getPatch().scene[s].modsource_doprocess[ms_breath])
                storage.getPatch().scene[s].modsources[ms_breath]->process_block();
            if (storage.getPatch().scene[s].modsource_doprocess[ms_expression])
                storage.getPatch().scene[s].modsources[ms_expression]->process_block();
            if (storage.getPatch().scene[s].modsource_doprocess[ms_sustain])
                storage.getPatch().scene[s].modsources[ms_sustain]->process_block();
            if (storage.getPatch().scene[s].modsource_doprocess[ms_aftertouch])
                storage.getPatch().scene[s].modsources[ms_aftertouch]->process_block();
            if (storage.getPatch().scene[s].modsource_doprocess[ms_lowest_key])
                storage.getPatch().scene[s].modsources[ms_lowest_key]->process_block();
            if (storage.getPatch().scene[s].modsource_doprocess[ms_highest_key])
                storage.getPatch().scene[s].modsources[ms_highest_key]->process_block();
            if (storage.getPatch().scene[s].modsource_doprocess[ms_latest_key])
                storage.getPatch().scene[s].modsources[ms_latest_key]->process_block();

            if (storage.getPatch().scene[s].modsource_doprocess[ms_random_unipolar])
                storage.getPatch().scene[s].modsources[ms_random_unipolar]->process_block();
            if (storage.getPatch().scene[s].modsource_doprocess[ms_random_bipolar])
                storage.getPatch().scene[s].modsources[ms_random_bipolar]->process_block();
            if (storage.getPatch().scene[s].modsource_doprocess[ms_alternate_bipolar])
                storage.getPatch().scene[s].modsources[ms_alternate_bipolar]->process_block();
            if (storage.getPatch().scene[s].modsource_doprocess[ms_alternate_unipolar])
                storage.getPatch().scene[s].modsources[ms_alternate_unipolar]->process_block();

            storage.getPatch().scene[s].modsources[ms_pitchbend]->process_block();

            for (int i = 0; i < n_customcontrollers; i++)
                storage.getPatch().scene[s].modsources[ms_ctrl1 + i]->process_block();

            // for(int i=0; i<n_lfos_scene; i++)
            // storage.getPatch().scene[s].modsources[ms_slfo1+i]->process_block();

            int n = storage.getPatch().scene[s].modulation_scene.size();
            for (int i = 0; i < n; i++)
            {
                int src_id = storage.getPatch().scene[s].modulation_scene[i].source_id;
                int src_index = storage.getPatch().scene[s].modulation_scene[i].source_index;
                if (storage.getPatch().scene[s].modsources[src_id])
                {
                    int dst_id = storage.getPatch().scene[s].modulation_scene[i].destination_id;
                    float depth = storage.getPatch().scene[s].modulation_scene[i].depth;
                    storage.getPatch().scenedata[s][dst_id].f +=
                        depth *
                        storage.getPatch().scene[s].modsources[src_id]->get_output(src_index) *
                        (1.0 - storage.getPatch().scene[s].modulation_scene[i].muted);
                }
            }

            for (int i = 0; i < n_lfos_scene; i++)
            {
                if (storage.getPatch().scene[s].lfo[n_lfos_voice + i].shape.val.i == lt_formula)
                {
                    auto lms = dynamic_cast<LFOModulationSource *>(
                        storage.getPatch().scene[s].modsources[ms_slfo1 + i]);
                    if (lms)
                    {
                        Surge::Formula::setupEvaluatorStateFrom(lms->formulastate,
                                                                storage.getPatch(), s);
                    }
                }
                storage.getPatch().scene[s].modsources[ms_slfo1 + i]->process_block();
            }
        }
    }

    loadOscalgos();

    int n = storage.getPatch().modulation_global.size();
    for (int i = 0; i < n; i++)
    {
        int src_id = storage.getPatch().modulation_global[i].source_id;
        int src_index = storage.getPatch().modulation_global[i].source_index;
        int dst_id = storage.getPatch().modulation_global[i].destination_id;
        float depth = storage.getPatch().modulation_global[i].depth;
        int source_scene = storage.getPatch().modulation_global[i].source_scene;

        storage.getPatch().globaldata[dst_id].f +=
            depth *
            storage.getPatch().scene[source_scene].modsources[src_id]->get_output(src_index) *
            (1 - storage.getPatch().modulation_global[i].muted);
    }

    if (switch_toggled_queued)
    {
        switch_toggled();
        switch_toggled_queued = false;
    }

    if (load_fx_needed)
        loadFx(false, false);

    if (fx_suspend_bitmask)
    {
        for (int i = 0; i < n_fx_slots; i++)
        {
            if (((1 << i) & fx_suspend_bitmask) && fx[i])
                fx[i]->suspend();
        }
        fx_suspend_bitmask = 0;
    }

    for (int i = 0; i < n_fx_slots; ++i)
        if (fx[i])
            refresh_editor |= fx[i]->checkHasInvalidatedUI();

#ifndef SURGE_SKIP_ODDSOUND_MTS
    if (storage.oddsound_mts_client)
    {
        storage.oddsound_mts_on_check = (storage.oddsound_mts_on_check + 1) & (1024 - 1);
        if (storage.oddsound_mts_on_check == 0)
        {
            if (storage.getPatch().dawExtraState.disconnectFromOddSoundMTS)
            {
                if (storage.oddsound_mts_active_as_client)
                {
                    auto q = storage.oddsound_mts_client;
                    storage.oddsound_mts_active_as_client = false;
                    storage.oddsound_mts_client = nullptr;
                    MTS_DeregisterClient(q);
                    refresh_editor = true;
                }
            }
            else
            {
                bool prior = storage.oddsound_mts_active_as_client;
                storage.setOddsoundMTSActiveTo(MTS_HasMaster(storage.oddsound_mts_client));

                if (prior != storage.oddsound_mts_active_as_client)
                {
                    refresh_editor = true;
                }
            }
        }
    }
#endif
}

void SurgeSynthesizer::process()
{
#if DEBUG_RNG_THREADING
    storage.audioThreadID = std::this_thread::get_id();
#endif
    processRunning = 0;

#if DEBUG
    memset(endedHostNoteIds, 0, 512 * sizeof(int32_t));
#endif

    auto process_start = std::chrono::high_resolution_clock::now();

    if (hostNoteEndedToPushToNextBlock)
    {
        for (int i = 0; i < hostNoteEndedToPushToNextBlock; ++i)
        {
            endedHostNoteIds[i] = nextBlockEndedHostNoteIds[i];
            endedHostNoteOriginalKey[i] = nextBlockEndedHostNoteOriginalKey[i];
            endedHostNoteOriginalChannel[i] = nextBlockEndedHostNoteOriginalChannel[i];
        }
        hostNoteEndedDuringBlockCount = hostNoteEndedToPushToNextBlock;
        hostNoteEndedToPushToNextBlock = 0;
    }
    else
    {
        hostNoteEndedToPushToNextBlock = 0;
        hostNoteEndedDuringBlockCount = 0;
    }

    float mfade = 1.f;

    if (halt_engine)
    {
        mech::clear_block<BLOCK_SIZE>(output[0]);
        mech::clear_block<BLOCK_SIZE>(output[1]);
        return;
    }
    else if (patchid_queue >= 0 || has_patchid_file)
    {
        masterfade = max(0.f, masterfade - 0.05f);
        mfade = masterfade * masterfade;

        if (masterfade < 0.0001f)
        {
            std::lock_guard<std::mutex> mg(patchLoadSpawnMutex);
            // spawn patch-loading thread
            stopSound();
            halt_engine = true;

            /*
             * In theory, since we only spawn under a lock and the loading thread
             * also holds that lock, this should "never" happen - namely the patch
             * load thread should be null always, and we null it out with a move
             * at the end of the thread operation.
             */
            if (patchLoadThread)
                patchLoadThread->join();

            patchLoadThread = std::make_unique<std::thread>(loadPatchInBackgroundThread, this);

            mech::clear_block<BLOCK_SIZE>(output[0]);
            mech::clear_block<BLOCK_SIZE>(output[1]);
            return;
        }
    }
    else if (approachingAllSoundOff)
    {
        masterfade = max(0.f, masterfade - 0.125f); // kill over 8 blocks
        mfade = masterfade * masterfade;

        if (masterfade < 0.0001f)
        {
            releaseScene(0);
            releaseScene(1);
            approachingAllSoundOff = false;
        }
    }

    // process inputs (upsample & halfrate)
    if (process_input)
    {
        sdsp::hardclip_block8<BLOCK_SIZE>(input[0]);
        sdsp::hardclip_block8<BLOCK_SIZE>(input[1]);
        mech::copy_from_to<BLOCK_SIZE>(input[0], storage.audio_in_nonOS[0]);
        mech::copy_from_to<BLOCK_SIZE>(input[1], storage.audio_in_nonOS[1]);
        halfbandIN.process_block_U2(input[0], input[1], storage.audio_in[0], storage.audio_in[1],
                                    BLOCK_SIZE_OS);
    }
    else
    {
        mech::clear_block<BLOCK_SIZE_OS>(storage.audio_in[0]);
        mech::clear_block<BLOCK_SIZE_OS>(storage.audio_in[1]);
        mech::clear_block<BLOCK_SIZE>(storage.audio_in_nonOS[1]);
        mech::clear_block<BLOCK_SIZE>(storage.audio_in_nonOS[1]);
    }

    // TODO: FIX SCENE ASSUMPTION
    float fxsendout alignas(16)[n_send_slots][2][BLOCK_SIZE];
    bool play_scene[n_scenes];

    {
        mech::clear_block<BLOCK_SIZE_OS>(sceneout[0][0]);
        mech::clear_block<BLOCK_SIZE_OS>(sceneout[0][1]);
        mech::clear_block<BLOCK_SIZE_OS>(sceneout[1][0]);
        mech::clear_block<BLOCK_SIZE_OS>(sceneout[1][1]);

        for (int i = 0; i < n_send_slots; ++i)
        {
            mech::clear_block<BLOCK_SIZE>(fxsendout[i][0]);
            mech::clear_block<BLOCK_SIZE>(fxsendout[i][1]);
        }
    }

    storage.modRoutingMutex.lock();
    processControl();

    amp.set_target_smoothed(
        storage.db_to_linear(storage.getPatch().globaldata[storage.getPatch().volume.id].f));
    amp_mute.set_target(mfade);

    int fx_bypass = storage.getPatch().fx_bypass.val.i;

    int sendToIndex[n_send_slots][2] = {
        {fxslot_send1, 0}, {fxslot_send2, 1}, {fxslot_send3, 2}, {fxslot_send4, 3}};

    if (fx_bypass == fxb_all_fx)
    {
        for (auto si : sendToIndex)
        {
            auto slot = si[0];
            auto idx = si[1];
            if (fx[slot])
            {
                FX[idx].set_target_smoothed(amp_to_linear(
                    storage.getPatch().globaldata[storage.getPatch().fx[slot].return_level.id].f));
                send[idx][0].set_target_smoothed(amp_to_linear(
                    storage.getPatch()
                        .scenedata[0][storage.getPatch().scene[0].send_level[idx].param_id_in_scene]
                        .f));
                send[idx][1].set_target_smoothed(amp_to_linear(
                    storage.getPatch()
                        .scenedata[1][storage.getPatch().scene[1].send_level[idx].param_id_in_scene]
                        .f));
            }
        }
    }

    list<SurgeVoice *>::iterator iter;

    for (int sc = 0; sc < n_scenes; sc++)
    {
        play_scene[sc] = (!voices[sc].empty());
    }

    int FBentry[n_scenes];
    int vcount = 0;

    for (int s = 0; s < n_scenes; s++)
    {
        FBentry[s] = 0;
        iter = voices[s].begin();
        while (iter != voices[s].end())
        {
            SurgeVoice *v = *iter;
            assert(v);
            bool resume = v->process_block(FBQ[s][FBentry[s] >> 2], FBentry[s] & 3);
            FBentry[s]++;

            vcount++;

            if (!resume)
            {
                freeVoice(v);
                iter = voices[s].erase(iter);
            }
            else
                iter++;
        }

        storage.modRoutingMutex.unlock();

        using sst::filters::FilterType, sst::filters::FilterSubType;
        fbq_global g;
        if (storage.getPatch().scene[s].filterunit[0].type.deactivated)
        {
            g.FU1ptr = nullptr;
        }
        else
        {
            g.FU1ptr = sst::filters::GetQFPtrFilterUnit(
                static_cast<FilterType>(storage.getPatch().scene[s].filterunit[0].type.val.i),
                static_cast<FilterSubType>(
                    storage.getPatch().scene[s].filterunit[0].subtype.val.i));
        }
        if (storage.getPatch().scene[s].filterunit[1].type.deactivated)
        {
            g.FU2ptr = nullptr;
        }
        else
        {
            g.FU2ptr = sst::filters::GetQFPtrFilterUnit(
                static_cast<FilterType>(storage.getPatch().scene[s].filterunit[1].type.val.i),
                static_cast<FilterSubType>(
                    storage.getPatch().scene[s].filterunit[1].subtype.val.i));
        }

        if (storage.getPatch().scene[s].wsunit.type.deactivated)
        {
            g.WSptr = nullptr;
        }
        else
        {
            g.WSptr =
                sst::waveshapers::GetQuadWaveshaper(static_cast<sst::waveshapers::WaveshaperType>(
                    storage.getPatch().scene[s].wsunit.type.val.i));
        }

        FBQFPtr ProcessQuadFB =
            GetFBQPointer(storage.getPatch().scene[s].filterblock_configuration.val.i,
                          g.FU1ptr != 0, g.WSptr != 0, g.FU2ptr != 0);

        for (int e = 0; e < FBentry[s]; e += 4)
        {
            int units = FBentry[s] - e;
            for (int i = units; i < 4; i++)
            {
                FBQ[s][e >> 2].FU[0].active[i] = 0;
                FBQ[s][e >> 2].FU[1].active[i] = 0;
                FBQ[s][e >> 2].FU[2].active[i] = 0;
                FBQ[s][e >> 2].FU[3].active[i] = 0;
            }
            ProcessQuadFB(FBQ[s][e >> 2], g, sceneout[s][0], sceneout[s][1]);
        }

        if (s == 0 && storage.otherscene_clients > 0)
        {
            // Make available for scene B
            mech::copy_from_to<BLOCK_SIZE_OS>(sceneout[0][0], storage.audio_otherscene[0]);
            mech::copy_from_to<BLOCK_SIZE_OS>(sceneout[0][1], storage.audio_otherscene[1]);
        }

        iter = voices[s].begin();

        while (iter != voices[s].end())
        {
            SurgeVoice *v = *iter;
            assert(v);
            v->GetQFB(); // save filter state in voices after quad processing is done
            iter++;
        }

        storage.modRoutingMutex.lock();

        // mute scene
        if (storage.getPatch().scene[s].volume.deactivated)
        {
            mech::clear_block<BLOCK_SIZE_OS>(sceneout[s][0]);
            mech::clear_block<BLOCK_SIZE_OS>(sceneout[s][1]);
        }
    }

    storage.modRoutingMutex.unlock();
    storage.activeVoiceCount = vcount;

    // TODO: FIX SCENE ASSUMPTION
    if (play_scene[0])
    {
        switch (storage.sceneHardclipMode[0])
        {
        case SurgeStorage::HARDCLIP_TO_18DBFS:
            sdsp::hardclip_block8<BLOCK_SIZE_OS>(sceneout[0][0]);
            sdsp::hardclip_block8<BLOCK_SIZE_OS>(sceneout[0][1]);
            break;
        case SurgeStorage::HARDCLIP_TO_0DBFS:
            sdsp::hardclip_block<BLOCK_SIZE_OS>(sceneout[0][0]);
            sdsp::hardclip_block<BLOCK_SIZE_OS>(sceneout[0][1]);
            break;
        case SurgeStorage::BYPASS_HARDCLIP:
            break;
        }

        halfbandA.process_block_D2(sceneout[0][0], sceneout[0][1], BLOCK_SIZE_OS);
    }

    if (play_scene[1])
    {
        switch (storage.sceneHardclipMode[1])
        {
        case SurgeStorage::HARDCLIP_TO_18DBFS:
            sdsp::hardclip_block8<BLOCK_SIZE_OS>(sceneout[1][0]);
            sdsp::hardclip_block8<BLOCK_SIZE_OS>(sceneout[1][1]);
            break;
        case SurgeStorage::HARDCLIP_TO_0DBFS:
            sdsp::hardclip_block<BLOCK_SIZE_OS>(sceneout[1][0]);
            sdsp::hardclip_block<BLOCK_SIZE_OS>(sceneout[1][1]);
            break;
        case SurgeStorage::BYPASS_HARDCLIP:
            break;
        }

        halfbandB.process_block_D2(sceneout[1][0], sceneout[1][1], BLOCK_SIZE_OS);
    }

    /*
     * ABOVE: Oversampled, Below, Regular sample. So BLOCK_SIZE_OS above BLOCK_SIZE below
     */

    // TODO: FIX SCENE ASSUMPTION
    if (storage.getPatch().scene[0].lowcut.deactivated == false)
    {
        auto freq =
            storage.getPatch().scenedata[0][storage.getPatch().scene[0].lowcut.param_id_in_scene].f;

        auto slope = storage.getPatch().scene[0].lowcut.deform_type;

        for (int i = 0; i <= slope; i++)
        {
            hpA[i].coeff_HP(hpA[i].calc_omega(freq / 12.0), 0.4); // var 0.707
            hpA[i].process_block(sceneout[0][0], sceneout[0][1]); // TODO: quadify
        }
    }

    if (storage.getPatch().scene[1].lowcut.deactivated == false)
    {
        auto freq =
            storage.getPatch().scenedata[1][storage.getPatch().scene[1].lowcut.param_id_in_scene].f;

        auto slope = storage.getPatch().scene[1].lowcut.deform_type;

        for (int i = 0; i <= slope; i++)
        {
            hpB[i].coeff_HP(hpB[i].calc_omega(freq / 12.0), 0.4); // var 0.707
            hpB[i].process_block(sceneout[1][0], sceneout[1][1]); // TODO: quadify
        }
    }

    for (int cls = 0; cls < n_scenes; ++cls)
    {
        switch (storage.sceneHardclipMode[cls])
        {
        case SurgeStorage::HARDCLIP_TO_18DBFS:
            sdsp::hardclip_block8<BLOCK_SIZE>(sceneout[cls][0]);
            sdsp::hardclip_block8<BLOCK_SIZE>(sceneout[cls][1]);
            break;
        case SurgeStorage::HARDCLIP_TO_0DBFS:
            sdsp::hardclip_block<BLOCK_SIZE>(sceneout[cls][0]);
            sdsp::hardclip_block<BLOCK_SIZE>(sceneout[cls][1]);
            break;
        default:
            break;
        }
    }

    // TODO: FIX SCENE ASSUMPTION
    bool sc_state[n_scenes];

    for (int i = 0; i < n_scenes; i++)
    {
        sc_state[i] = play_scene[i];
    }

    for (int i = 0; i < n_scenes; i++)
        for (int channel = 0; channel < N_OUTPUTS; channel++)
            storage.scenesOutputData.provideSceneData(i, channel, sceneout[i][channel]);

    // apply insert effects
    if (fx_bypass != fxb_no_fx)
    {
        for (auto v : {fxslot_ains1, fxslot_ains2, fxslot_ains3, fxslot_ains4})
        {
            if (fx[v] && !(storage.getPatch().fx_disable.val.i & (1 << v)))
            {
                sc_state[0] = fx[v]->process_ringout(sceneout[0][0], sceneout[0][1], sc_state[0]);
            }
        }

        for (auto v : {fxslot_bins1, fxslot_bins2, fxslot_bins3, fxslot_bins4})
        {
            if (fx[v] && !(storage.getPatch().fx_disable.val.i & (1 << v)))
            {
                sc_state[1] = fx[v]->process_ringout(sceneout[1][0], sceneout[1][1], sc_state[1]);
            }
        }
    }

    for (int cls = 0; cls < n_scenes; ++cls)
    {
        switch (storage.sceneHardclipMode[cls])
        {
        case SurgeStorage::HARDCLIP_TO_18DBFS:
            sdsp::hardclip_block8<BLOCK_SIZE>(sceneout[cls][0]);
            sdsp::hardclip_block8<BLOCK_SIZE>(sceneout[cls][1]);
            break;
        case SurgeStorage::HARDCLIP_TO_0DBFS:
            sdsp::hardclip_block<BLOCK_SIZE>(sceneout[cls][0]);
            sdsp::hardclip_block<BLOCK_SIZE>(sceneout[cls][1]);
            break;
        default:
            break;
        }
    }

    // sum scenes
    // TODO: FIX SCENE ASSUMPTION
    mech::copy_from_to<BLOCK_SIZE>(sceneout[0][0], output[0]);
    mech::copy_from_to<BLOCK_SIZE>(sceneout[0][1], output[1]);
    mech::accumulate_from_to<BLOCK_SIZE>(sceneout[1][0], output[0]);
    mech::accumulate_from_to<BLOCK_SIZE>(sceneout[1][1], output[1]);

    bool sendused[4] = {false, false, false, false};
    // add send effects
    // TODO: FIX SCENE ASSUMPTION
    if (fx_bypass == fxb_all_fx)
    {
        for (auto si : sendToIndex)
        {
            auto slot = si[0];
            auto idx = si[1];

            if (fx[slot] && !(storage.getPatch().fx_disable.val.i & (1 << slot)))
            {
                send[idx][0].MAC_2_blocks_to(sceneout[0][0], sceneout[0][1], fxsendout[idx][0],
                                             fxsendout[idx][1], BLOCK_SIZE_QUAD);
                send[idx][1].MAC_2_blocks_to(sceneout[1][0], sceneout[1][1], fxsendout[idx][0],
                                             fxsendout[idx][1], BLOCK_SIZE_QUAD);
                sendused[idx] = fx[slot]->process_ringout(fxsendout[idx][0], fxsendout[idx][1],
                                                          sc_state[0] || sc_state[1]);
                FX[idx].MAC_2_blocks_to(fxsendout[idx][0], fxsendout[idx][1], output[0], output[1],
                                        BLOCK_SIZE_QUAD);
            }
        }
    }

    // apply global effects
    if ((fx_bypass == fxb_all_fx) || (fx_bypass == fxb_no_sends))
    {
        bool glob = sc_state[0] || sc_state[1];
        for (int i = 0; i < n_send_slots; ++i)
            glob = glob || sendused[i];

        for (auto v : {fxslot_global1, fxslot_global2, fxslot_global3, fxslot_global4})
        {
            if (fx[v] && !(storage.getPatch().fx_disable.val.i & (1 << v)))
            {
                glob = fx[v]->process_ringout(output[0], output[1], glob);
            }
        }
    }

    amp.multiply_2_blocks(output[0], output[1], BLOCK_SIZE_QUAD);
    amp_mute.multiply_2_blocks(output[0], output[1], BLOCK_SIZE_QUAD);

    // VU falloff
    float a = storage.vu_falloff;
    vu_peak[0] = min(2.f, a * vu_peak[0]);
    vu_peak[1] = min(2.f, a * vu_peak[1]);
    vu_peak[0] = max(vu_peak[0], mech::blockAbsMax<BLOCK_SIZE>(output[0]));
    vu_peak[1] = max(vu_peak[1], mech::blockAbsMax<BLOCK_SIZE>(output[1]));

    switch (storage.hardclipMode)
    {
    case SurgeStorage::HARDCLIP_TO_18DBFS:
        sdsp::hardclip_block8<BLOCK_SIZE>(output[0]);
        sdsp::hardclip_block8<BLOCK_SIZE>(output[1]);
        break;

    case SurgeStorage::HARDCLIP_TO_0DBFS:
        sdsp::hardclip_block<BLOCK_SIZE>(output[0]);
        sdsp::hardclip_block<BLOCK_SIZE>(output[1]);
        break;
    case SurgeStorage::BYPASS_HARDCLIP:
        break;
    }

    // Send output to the oscilloscope, if anyone is listening.
    if (storage.audioOut.subscribed())
    {
        storage.audioOut.push(output[0], output[1], BLOCK_SIZE);
    }

    // since the sceneout is now routable we also need to mute it
    for (int sc = 0; sc < n_scenes; ++sc)
    {
        amp_mute.multiply_2_blocks(sceneout[sc][0], sceneout[sc][1], BLOCK_SIZE_QUAD);
    }

    // Calculate how close we are to overloading the CPU
    // (how close is the process() duration to duration)
    auto process_end = std::chrono::high_resolution_clock::now();
    auto duration_usec =
        std::chrono::duration_cast<std::chrono::microseconds>(process_end - process_start);
    auto max_duration_usec = BLOCK_SIZE * storage.dsamplerate_inv * 1000000;
    float ratio = duration_usec.count() / max_duration_usec;
    float c = cpu_level.load();
    int window = max_duration_usec;
    auto smoothed_ratio = (c * (window - 1) + ratio) / window;
    c = c * storage.cpu_falloff;
    cpu_level.store(max(c, smoothed_ratio));
}

SurgeSynthesizer::PluginLayer *SurgeSynthesizer::getParent()
{
    assert(_parent != nullptr);
    return _parent;
}

void SurgeSynthesizer::populateDawExtraState()
{
    auto &des = storage.getPatch().dawExtraState;

    des.isPopulated = true;

    des.oscPortIn = storage.oscPortIn;
    des.oscPortOut = storage.oscPortOut;
    des.oscIPAddrOut = storage.oscOutIP;
    des.oscStartIn = storage.oscStartIn;
    des.oscStartOut = storage.oscStartOut;

    des.mpeEnabled = mpeEnabled;
    des.mpePitchBendRange = storage.mpePitchBendRange;
    des.mpeTimbreIsUnipolar = mpeTimbreIsUnipolar;

    des.isDirty = storage.getPatch().isDirty;

    des.hasScale = !storage.isStandardScale;
    des.scaleContents = (!storage.isStandardScale) ? storage.currentScale.rawText : "";

    des.hasMapping = !storage.isStandardMapping;

    if (!storage.isStandardMapping)
    {
        des.mappingContents = storage.currentMapping.rawText;
        des.mappingName = storage.currentMapping.name;
    }
    else
    {
        des.mappingContents = "";
        des.mappingName = "";
    }

    des.mapChannelToOctave = storage.mapChannelToOctave;

    int n = n_global_params + (n_scene_params * n_scenes);

    for (int i = 0; i < n; i++)
    {
        if (storage.getPatch().param_ptr[i]->midictrl >= 0)
        {
            des.midictrl_map[i] = storage.getPatch().param_ptr[i]->midictrl;
        }

        if (storage.getPatch().param_ptr[i]->midichan >= -1)
        {
            des.midichan_map[i] = storage.getPatch().param_ptr[i]->midichan;
        }
    }

    for (int i = 0; i < n_customcontrollers; ++i)
    {
        des.customcontrol_map[i] = storage.controllers[i];
        des.customcontrol_chan_map[i] = storage.controllers_chan[i];
    }

    des.monoPedalMode = storage.monoPedalMode;
    des.oddsoundRetuneMode = storage.oddsoundRetuneMode;

    des.lastLoadedPatch = storage.lastLoadedPatch;
}

void SurgeSynthesizer::loadFromDawExtraState()
{
    auto &des = storage.getPatch().dawExtraState;

    if (!des.isPopulated)
    {
        return;
    }

    mpeEnabled = des.mpeEnabled;

    storage.oscPortIn = des.oscPortIn;
    storage.oscPortOut = des.oscPortOut;
    storage.oscOutIP = des.oscIPAddrOut;
    storage.oscStartIn = des.oscStartIn;
    storage.oscStartOut = des.oscStartOut;

    if (des.mpePitchBendRange > 0)
    {
        storage.mpePitchBendRange = des.mpePitchBendRange;
    }

    mpeTimbreIsUnipolar = des.mpeTimbreIsUnipolar;

    storage.getPatch().isDirty = des.isDirty;

    storage.monoPedalMode = (MonoPedalMode)des.monoPedalMode;
    storage.oddsoundRetuneMode = (SurgeStorage::OddsoundRetuneMode)des.oddsoundRetuneMode;

    if (des.hasScale)
    {
        try
        {
            auto sc = Tunings::parseSCLData(des.scaleContents);
            storage.retuneToScale(sc);
        }
        catch (Tunings::TuningError &e)
        {
            storage.reportError(e.what(), "Unable to restore tuning!");
            storage.retuneTo12TETScale();
        }
    }
    else
    {
        storage.retuneTo12TETScale();
    }

    if (des.hasMapping)
    {
        try
        {
            auto kb = Tunings::parseKBMData(des.mappingContents);

            if (des.mappingName.size() > 1)
            {
                kb.name = des.mappingName;
            }
            else
            {
                kb.name = storage.guessAtKBMName(kb);
            }
            storage.remapToKeyboard(kb);
        }
        catch (Tunings::TuningError &e)
        {
            storage.reportError(e.what(), "Unable to restore mapping!");
            storage.remapToConcertCKeyboard();
        }
    }
    else
    {
        storage.remapToConcertCKeyboard();
    }

    storage.mapChannelToOctave = des.mapChannelToOctave;

    int n = n_global_params + (n_scene_params * n_scenes);
    int nOld = n_global_params + n_scene_params;

    for (int i = 0; i < n; i++)
    {
        if (des.midictrl_map.find(i) != des.midictrl_map.end())
        {
            storage.getPatch().param_ptr[i]->midictrl = des.midictrl_map[i];

            // if we're loading old DAW extra state, midichan_map should be empty
            // in this case, duplicate scene A assignments to scene B to retain old behavior
            // this gets fixed up in populateDawExtraState() and when DAW project is resaved
            if (i >= n_global_params && i < nOld && des.midichan_map.empty())
            {
                storage.getPatch().param_ptr[i + n_scene_params]->midictrl = des.midictrl_map[i];
            }
        }

        if (des.midichan_map.find(i) != des.midichan_map.end())
        {
            storage.getPatch().param_ptr[i]->midichan = des.midichan_map[i];
        }
    }

    for (int i = 0; i < n_customcontrollers; ++i)
    {
        if (des.customcontrol_map.find(i) != des.customcontrol_map.end())
        {
            storage.controllers[i] = des.customcontrol_map[i];
        }

        if (des.customcontrol_chan_map.find(i) != des.customcontrol_chan_map.end())
        {
            storage.controllers_chan[i] = des.customcontrol_chan_map[i];
        }
    }

    storage.lastLoadedPatch = des.lastLoadedPatch;
}

void SurgeSynthesizer::swapMetaControllers(int c1, int c2)
{
    char nt[CUSTOM_CONTROLLER_LABEL_SIZE + 4];
    strxcpy(nt, storage.getPatch().CustomControllerLabel[c1], CUSTOM_CONTROLLER_LABEL_SIZE);
    strxcpy(storage.getPatch().CustomControllerLabel[c1],
            storage.getPatch().CustomControllerLabel[c2], CUSTOM_CONTROLLER_LABEL_SIZE);
    strxcpy(storage.getPatch().CustomControllerLabel[c2], nt, CUSTOM_CONTROLLER_LABEL_SIZE);

    storage.modRoutingMutex.lock();

    auto tmp1 = storage.getPatch().scene[0].modsources[ms_ctrl1 + c1];
    auto tmp2 = storage.getPatch().scene[0].modsources[ms_ctrl1 + c2];

    for (int sc = 0; sc < n_scenes; ++sc)
    {
        storage.getPatch().scene[sc].modsources[ms_ctrl1 + c2] = tmp1;
        storage.getPatch().scene[sc].modsources[ms_ctrl1 + c1] = tmp2;
    }

    // Now swap the routings
    for (int sc = 0; sc < n_scenes; ++sc)
    {
        for (int vt = 0; vt < 3; ++vt)
        {
            std::vector<ModulationRouting> *mv = nullptr;
            if (sc == 0 && vt == 0)
            {
                mv = &(storage.getPatch().modulation_global);
            }
            else if (vt == 1)
            {
                mv = &(storage.getPatch().scene[sc].modulation_scene);
            }
            else if (vt == 2)
            {
                mv = &(storage.getPatch().scene[sc].modulation_voice);
            }

            if (mv)
            {
                const int n = mv->size();
                for (int i = 0; i < n; ++i)
                {
                    auto &mati = mv->at(i);
                    if (mati.source_id == ms_ctrl1 + c1)
                    {
                        mati.source_id = ms_ctrl1 + c2;
                    }
                    else if (mati.source_id == ms_ctrl1 + c2)
                    {
                        mati.source_id = ms_ctrl1 + c1;
                    }
                }
            }
        }
    }

    storage.modRoutingMutex.unlock();

    refresh_editor = true;
}

void SurgeSynthesizer::changeModulatorSmoothing(Modulator::SmoothingMode m)
{
    storage.smoothingMode = m;
    for (int sc = 0; sc < n_scenes; ++sc)
    {
        for (int q = 0; q < n_modsources; ++q)
        {
            auto cms = dynamic_cast<ControllerModulationSource *>(
                storage.getPatch().scene[sc].modsources[q]);
            if (cms)
            {
                cms->smoothingMode = m;
            }
        }
    }
}

void SurgeSynthesizer::reorderFx(int source, int target, FXReorderMode m)
{
    if (source < 0 || source >= n_fx_slots || target < 0 || target >= n_fx_slots ||
        source == target)
    {
        return;
    }

    std::lock_guard<std::recursive_mutex> lockModulation(storage.modRoutingMutex);

    FxStorage so{storage.getPatch().fx[source]};
    FxStorage to{storage.getPatch().fx[target]};

    fxmodsync[source].clear();
    fxmodsync[target].clear();

    fxsync[target].type.val.i = so.type.val.i;

    Effect *t_fx = spawn_effect(fxsync[target].type.val.i, &storage, &fxsync[target], 0);

    if (t_fx)
    {
        t_fx->init_ctrltypes();
        t_fx->init_default_values();
        delete t_fx;
    }

    switch (m)
    {
    case FXReorderMode::NONE:
    {
        if (source == target)
        {
            fxsync[source].type.val.i = 0;
            fxsync[target].type.val.i = 0;
        }
    }
    break;
    case FXReorderMode::MOVE:
    {
        fxsync[source].type.val.i = 0;
    }
    break;
    case FXReorderMode::SWAP:
    {
        fxsync[source].type.val.i = to.type.val.i;

        t_fx = spawn_effect(fxsync[source].type.val.i, &storage, &fxsync[source], 0);

        if (t_fx)
        {
            t_fx->init_ctrltypes();
            t_fx->init_default_values();
            delete t_fx;
        }
    }
    default:
        break;
    }

    /*
     * OK we can't copy the params - they contain things like id in scene - we need to copy the
     * values
     */
    auto cp = [](Parameter &to, const Parameter &from) {
        to.set_extend_range(from.extend_range);
        to.temposync = from.temposync;
        to.deactivated = from.deactivated;
        to.absolute = from.absolute;
        to.val = from.val;
    };

    for (int i = 0; i < n_fx_params; ++i)
    {
        if (m == FXReorderMode::SWAP)
        {
            cp(fxsync[source].p[i], to.p[i]);
        }

        cp(fxsync[target].p[i], so.p[i]);
    }

    // Now swap the routings. FX routings are always global
    std::vector<ModulationRouting> *mv = nullptr;
    mv = &(storage.getPatch().modulation_global);
    int n = mv->size();
    std::vector<int> deleteThese;

    for (int i = 0; i < n; ++i)
    {
        if (mv->at(i).destination_id >= fxsync[source].p[0].id &&
            mv->at(i).destination_id <= fxsync[source].p[n_fx_params - 1].id)
        {
            fx_reload_mod[target] = true;
            int whichForReal = -1;

            for (int q = 0; q < n_fx_params; ++q)
            {
                if (mv->at(i).destination_id == fxsync[source].p[q].id)
                {
                    whichForReal = q;
                }
            }

            auto depth =
                getModDepth01(fxsync[source].p[whichForReal].id, (modsources)mv->at(i).source_id,
                              mv->at(i).source_scene, mv->at(i).source_index);
            auto mut = isModulationMuted(fxsync[source].p[whichForReal].id,
                                         (modsources)mv->at(i).source_id, mv->at(i).source_scene,
                                         mv->at(i).source_index);
            fxmodsync[target].push_back({mv->at(i).source_id, mv->at(i).source_scene,
                                         mv->at(i).source_index, whichForReal, depth, mut});
        }

        if (m == FXReorderMode::SWAP)
        {
            if (mv->at(i).destination_id >= fxsync[target].p[0].id &&
                mv->at(i).destination_id <= fxsync[target].p[n_fx_params - 1].id)
            {
                fx_reload_mod[source] = true;
                int whichForReal = -1;

                for (int q = 0; q < n_fx_params; ++q)
                {
                    if (mv->at(i).destination_id == fxsync[target].p[q].id)
                    {
                        whichForReal = q;
                    }
                }

                auto depth = getModDepth01(fxsync[target].p[whichForReal].id,
                                           (modsources)mv->at(i).source_id, mv->at(i).source_scene,
                                           mv->at(i).source_index);
                auto mut = isModulationMuted(fxsync[target].p[whichForReal].id,
                                             (modsources)mv->at(i).source_id,
                                             mv->at(i).source_scene, mv->at(i).source_index);
                fxmodsync[source].push_back({mv->at(i).source_id, mv->at(i).source_scene,
                                             mv->at(i).source_index, whichForReal, depth, mut});
            }
        }

        // Any residual target modulation has to go
        if (mv->at(i).destination_id >= fxsync[target].p[0].id &&
            mv->at(i).destination_id <= fxsync[target].p[n_fx_params - 1].id)
        {
            deleteThese.push_back(i);
        }

        if (m == FXReorderMode::MOVE || m == FXReorderMode::SWAP)
        {
            // and if we are moving or swapping delete anything we left behind
            if (mv->at(i).destination_id >= fxsync[source].p[0].id &&
                mv->at(i).destination_id <= fxsync[source].p[n_fx_params - 1].id)
            {
                deleteThese.push_back(i);
            }
        }
    }

    for (auto dt = deleteThese.rbegin(); dt != deleteThese.rend(); dt++)
    {
        mv->erase(mv->begin() + *dt);
    }

    if (m != FXReorderMode::COPY)
    {
        fx_reload[source] = true;
    }

    // Finally deal with the bitmask
    auto startingBitmask = storage.getPatch().fx_disable.val.i;
    auto sourceBitmask = (startingBitmask & (1 << source)) ? 1 : 0;
    auto targetBitmask = (startingBitmask & (1 << target)) ? 1 : 0;

    if (m == FXReorderMode::SWAP)
    {
        // target to source and source to target
        startingBitmask = startingBitmask & ~(1 << source) & ~(1 << target);
        startingBitmask = startingBitmask | (sourceBitmask << target) | (targetBitmask << source);
    }
    else if (m == FXReorderMode::MOVE)
    {
        // source bitmask goes back to off
        startingBitmask =
            (startingBitmask & ~(1 << source) & ~(1 << target)) | (sourceBitmask << target);
    }
    else
    {
        // It's a copy
        startingBitmask = (startingBitmask & ~(1 << target)) | (sourceBitmask << target);
    }

    storage.getPatch().fx_disable.val.i = startingBitmask;
    fx_suspend_bitmask = startingBitmask;

    fx_reload[target] = true;
    load_fx_needed = true;
    refresh_editor = true;
}

bool SurgeSynthesizer::getParameterIsBoolean(const ID &id) const
{
    auto index = id.getSynthSideId();

    if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
    {
        auto t = storage.getPatch().param_ptr[index]->valtype;
        if (t == vt_bool)
            return true;
    }

    return false;
}

void SurgeSynthesizer::reclaimVoiceFor(SurgeVoice *v, char key, char channel, char velocity,
                                       int scene, int host_noteid, int host_originating_channel,
                                       int host_originating_key, bool resetToZero)
{
    float aegStart{0.f}, fegStart{0.};
    if (!resetToZero)
        v->getAEGFEGLevel(aegStart, fegStart);

    auto priorNoteId = v->host_note_id;
    auto priorChannel = v->originating_host_channel;
    auto priorKey = v->originating_host_key;

    v->state.gate = true;
    v->state.key = key;
    v->state.uberrelease = false;
    v->state.channel = (unsigned char)channel;
    v->state.voiceChannelState = &channelState[channel];

    v->host_note_id = host_noteid;
    v->originating_host_channel = host_originating_channel;
    v->originating_host_key = host_originating_key;

    channelState[channel].keyState[key].voiceOrder = voiceCounter++;

    v->resetVelocity((unsigned int)velocity);
    v->restartAEGFEGAttack(aegStart, fegStart);
    v->retriggerLFOEnvelopes();
    v->retriggerOSCWithIndependentAttacks();
    v->resetPortamentoFrom(priorKey, channel);

    // Now end this note unless it is used by another scene
    bool endHostVoice = true;
    for (auto s = 0; s < n_scenes; ++s)
    {
        if (s == scene)
            continue;
        for (auto v : voices[s])
        {
            if (v->host_note_id == priorNoteId)
            {
                endHostVoice = false;
            }
        }
    }
    if (endHostVoice)
        notifyEndedNote(priorNoteId, priorKey, priorChannel, false);
}
