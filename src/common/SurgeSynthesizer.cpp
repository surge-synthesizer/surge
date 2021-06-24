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

#include "SurgeSynthesizer.h"
#include "DSPUtils.h"
#include <ctime>
#include "CPUFeatures.h"
#if MAC || LINUX
#include <pthread.h>
#else
#include <windows.h>
#include <process.h>
#endif

#include "SurgeParamConfig.h"

#include "UserDefaults.h"
#include "filesystem/import.h"
#include "Effect.h"

#include <thread>
#include "libMTSClient.h"

using namespace std;

SurgeSynthesizer::SurgeSynthesizer(PluginLayer *parent, std::string suppliedDataPath)
    : storage(suppliedDataPath), hpA(&storage), hpB(&storage), _parent(parent), halfbandA(6, true),
      halfbandB(6, true), halfbandIN(6, true)
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

    demo_counter = 10;

    // TODO: FIX NUMBER OF FX ASSUMPTION
    memset(fx, 0, sizeof(void *) * 8);
    srand((unsigned)time(nullptr));
    // TODO: FIX SCENE ASSUMPTION
    memset(storage.getPatch().scenedata[0], 0, sizeof(pdata) * n_scene_params);
    memset(storage.getPatch().scenedata[1], 0, sizeof(pdata) * n_scene_params);
    memset(storage.getPatch().globaldata, 0, sizeof(pdata) * n_global_params);
    memset(mControlInterpolatorUsed, 0, sizeof(bool) * num_controlinterpolators);
    memset((void *)fxsync, 0, sizeof(FxStorage) * n_fx_slots);

    for (int i = 0; i < n_fx_slots; i++)
    {
        memcpy((void *)&fxsync[i], (void *)&storage.getPatch().fx[i], sizeof(FxStorage));
        fx_reload[i] = false;
        fx_reload_mod[i] = false;
    }

    allNotesOff();

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

    storage.smoothingMode =
        (ControllerModulationSource::SmoothingMode)(int)Surge::Storage::getUserDefaultValue(
            &storage, Surge::Storage::SmoothingMode,
            (int)(ControllerModulationSource::SmoothingMode::LEGACY));
    storage.pitchSmoothingMode =
        (ControllerModulationSource::SmoothingMode)(int)Surge::Storage::getUserDefaultValue(
            &storage, Surge::Storage::PitchSmoothingMode,
            (int)(ControllerModulationSource::SmoothingMode::DIRECT));

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
        scene.modsources[ms_lowest_key] = new ControllerModulationSource(storage.smoothingMode);
        scene.modsources[ms_highest_key] = new ControllerModulationSource(storage.smoothingMode);
        scene.modsources[ms_latest_key] = new ControllerModulationSource(storage.smoothingMode);

        ((ControllerModulationSource *)scene.modsources[ms_lowest_key])->init(0.f);
        ((ControllerModulationSource *)scene.modsources[ms_highest_key])->init(0.f);
        ((ControllerModulationSource *)scene.modsources[ms_latest_key])->init(0.f);

        scene.modsources[ms_random_bipolar] = new RandomModulationSource(true);
        scene.modsources[ms_random_unipolar] = new RandomModulationSource(false);
        scene.modsources[ms_alternate_bipolar] = new AlternateModulationSource(true);
        scene.modsources[ms_alternate_unipolar] = new AlternateModulationSource(false);

        for (int osc = 0; osc < n_oscs; osc++)
        {
            // p 5 is unison detune
            scene.osc[osc].p[5].val.f = 0.1f;
        }

        for (int i = 0; i < n_filterunits_per_scene; i++)
        {
            scene.filterunit[i].type.set_user_data(&patch.patchFilterSelectorMapper);
        }

        scene.filterblock_configuration.val.i = fc_wide;

        for (int l = 0; l < n_lfos_scene; l++)
        {
            scene.modsources[ms_slfo1 + l] = new LFOModulationSource();
            ((LFOModulationSource *)scene.modsources[ms_slfo1 + l])
                ->assign(&storage, &scene.lfo[n_lfos_voice + l], storage.getPatch().scenedata[sc],
                         0, &patch.stepsequences[sc][n_lfos_voice + l],
                         &patch.msegs[sc][n_lfos_voice + l],
                         &patch.formulamods[sc][n_lfos_voice + l]);
        }

        for (int k = 0; k < 128; ++k)
            midiKeyPressedForScene[sc][k] = 0;
    }

    for (int i = 0; i < n_customcontrollers; i++)
    {
        patch.scene[0].modsources[ms_ctrl1 + i] =
            new ControllerModulationSource(storage.smoothingMode);

        for (int j = 1; j < n_scenes; j++)
        {
            patch.scene[j].modsources[ms_ctrl1 + i] = patch.scene[0].modsources[ms_ctrl1 + i];
        }
    }

    amp.set_blocksize(BLOCK_SIZE);
    // TODO: FIX SCENE ASSUMPTION
    FX1.set_blocksize(BLOCK_SIZE);
    FX2.set_blocksize(BLOCK_SIZE);
    send[0][0].set_blocksize(BLOCK_SIZE);
    send[0][1].set_blocksize(BLOCK_SIZE);
    send[1][0].set_blocksize(BLOCK_SIZE);
    send[1][1].set_blocksize(BLOCK_SIZE);

    polydisplay = 0;
    refresh_editor = false;
    patch_loaded = false;
    storage.getPatch().category = "Init";
    storage.getPatch().name = "Init";
    storage.getPatch().comment = "";
    storage.getPatch().author = "Surge Synth Team";
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

    learn_param = -1;
    learn_custom = -1;

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
    for (auto p : storage.patch_list)
    {
        if (p.name == storage.initPatchName &&
            storage.patch_category[p.category].name == storage.initPatchCategory)
        {
            patchid_queue = pid;
            break;
        }
        pid++;
    }
    if (patchid_queue >= 0)
        processThreadunsafeOperations(true); // DANGER MODE IS ON
    patchid_queue = -1;
}

SurgeSynthesizer::~SurgeSynthesizer()
{
    allNotesOff();

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

int SurgeSynthesizer::calculateChannelMask(int channel, int key)
{
    /*
    ** Just because I always forget
    **
    ** A voice is routed to scene n if channelmask & n. So "1" means scene A, "2" means scene B and
    *"3" (= 2 | 1 ) = both.
    */
    int channelmask = channel;

    if ((channel == 0) || (channel > 2) || mpeEnabled ||
        storage.getPatch().scenemode.val.i == sm_chsplit)
    {
        switch (storage.getPatch().scenemode.val.i)
        {
        case sm_single:
            //	case sm_morph:
            if (storage.getPatch().scene_active.val.i == 1)
                channelmask = 2;
            else
                channelmask = 1;
            break;
        case sm_dual:
            channelmask = 3;
            break;
        case sm_split:
            if (key < storage.getPatch().splitpoint.val.i)
                channelmask = 1;
            else
                channelmask = 2;
            break;
        case sm_chsplit:
            if (channel < ((int)(storage.getPatch().splitpoint.val.i / 8) + 1))
                channelmask = 1;
            else
                channelmask = 2;

            break;
        }
    }
    else if (storage.getPatch().scenemode.val.i == sm_single)
    {
        if (storage.getPatch().scene_active.val.i == 1)
            channelmask = 2;
        else
            channelmask = 1;
    }

    return channelmask;
}

void SurgeSynthesizer::playNote(char channel, char key, char velocity, char detune)
{
    if (halt_engine)
        return;

    if (storage.oddsound_mts_client && storage.oddsound_mts_active)
    {
        if (MTS_ShouldFilterNote(storage.oddsound_mts_client, key, channel))
        {
            return;
        }
    }

    // For split/dual
    // MIDI Channel 1 plays the split/dual
    // MIDI Channel 2 plays A
    // MIDI Channel 3 plays B

    int channelmask = calculateChannelMask(channel, key);

    // TODO: FIX SCENE ASSUMPTION
    if (channelmask & 1)
    {
        midiKeyPressedForScene[0][key] = ++orderedMidiKey;
        playVoice(0, channel, key, velocity, detune);
    }
    if (channelmask & 2)
    {
        midiKeyPressedForScene[1][key] = ++orderedMidiKey;
        playVoice(1, channel, key, velocity, detune);
    }

    channelState[channel].keyState[key].keystate = velocity;
    channelState[channel].keyState[key].lastdetune = detune;

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
                if (h.first == channel && h.second == key)
                {
                    h.first = -1;
                    h.second = -1;
                }
            }
        }
    }
}

void SurgeSynthesizer::softkillVoice(int s)
{
    list<SurgeVoice *>::iterator iter, max_playing, max_released;
    int max_age = 0, max_age_release = 0;
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
    if (max_age_release)
        (*max_released)->uber_release();
    else if (max_age)
        (*max_playing)->uber_release();
}

// only allow 'margin' number of voices to be softkilled simultaneously
void SurgeSynthesizer::enforcePolyphonyLimit(int s, int margin)
{
    list<SurgeVoice *>::iterator iter;

    if (voices[s].size() > (storage.getPatch().polylimit.val.i + margin))
    {
        int excess_voices =
            max(0, (int)voices[s].size() - (storage.getPatch().polylimit.val.i + margin));
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

int SurgeSynthesizer::getNonUltrareleaseVoices(int s)
{
    int count = 0;

    list<SurgeVoice *>::iterator iter;
    iter = voices[s].begin();
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

int SurgeSynthesizer::getNonReleasedVoices(int s)
{
    int count = 0;

    list<SurgeVoice *>::iterator iter;
    iter = voices[s].begin();
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
    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (voices_usedby[0][i] && (v == &voices_array[0][i]))
        {
            voices_usedby[0][i] = 0;
        }
        if (voices_usedby[1][i] && (v == &voices_array[1][i]))
        {
            voices_usedby[1][i] = 0;
        }
    }
    v->freeAllocatedElements();
}

int SurgeSynthesizer::getMpeMainChannel(int voiceChannel, int key)
{
    if (mpeEnabled)
    {
        return 0;
    }

    return voiceChannel;
}

void SurgeSynthesizer::playVoice(int scene, char channel, char key, char velocity, char detune)
{
    if (getNonReleasedVoices(scene) == 0)
    {
        for (int l = 0; l < n_lfos_scene; l++)
        {
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

    switch (storage.getPatch().scene[scene].polymode.val.i)
    {
    case pm_poly:
    {
        SurgeVoice *nvoice = getUnusedVoice(scene);
        if (nvoice)
        {
            int mpeMainChannel = getMpeMainChannel(channel, key);

            voices[scene].push_back(nvoice);
            new (nvoice) SurgeVoice(
                &storage, &storage.getPatch().scene[scene], storage.getPatch().scenedata[scene],
                key, velocity, channel, scene, detune, &channelState[channel].keyState[key],
                &channelState[mpeMainChannel], &channelState[channel], mpeEnabled, voiceCounter++);
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
            for (iter = voices[scene].begin(); iter != voices[scene].end(); iter++)
            {
                SurgeVoice *v = *iter;
                if (v->state.scene_id == scene)
                {
                    if (v->state.gate)
                    {
                        glide = true;
                    }
                    v->uber_release();
                }
            }
            SurgeVoice *nvoice = getUnusedVoice(scene);
            if (nvoice)
            {
                int mpeMainChannel = getMpeMainChannel(channel, key);

                voices[scene].push_back(nvoice);
                if ((storage.getPatch().scene[scene].polymode.val.i == pm_mono_fp) && !glide)
                    storage.last_key[scene] = key;
                new (nvoice) SurgeVoice(&storage, &storage.getPatch().scene[scene],
                                        storage.getPatch().scenedata[scene], key, velocity, channel,
                                        scene, detune, &channelState[channel].keyState[key],
                                        &channelState[mpeMainChannel], &channelState[channel],
                                        mpeEnabled, voiceCounter++);
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
            for (iter = voices[scene].begin(); iter != voices[scene].end(); iter++)
            {
                SurgeVoice *v = *iter;
                if ((v->state.scene_id == scene) && (v->state.gate))
                {
                    v->legato(key, velocity, detune);
                    found_one = true;
                    if (mpeEnabled)
                    {
                        /*
                        ** This voice was created on a channel but is being legato held to another
                        *channel
                        ** so it needs to borrow the channel and channelState. Obviously this can
                        *only
                        ** happen in MPE mode.
                        */
                        v->state.channel = channel;
                        v->state.voiceChannelState = &channelState[channel];
                    }
                    break;
                }
                else
                {
                    if (v->state.scene_id == scene)
                        v->uber_release(); // make this optional for poly legato
                }
            }
            if (!found_one)
            {
                int mpeMainChannel = getMpeMainChannel(channel, key);

                SurgeVoice *nvoice = getUnusedVoice(scene);
                if (nvoice)
                {
                    voices[scene].push_back(nvoice);
                    new (nvoice) SurgeVoice(
                        &storage, &storage.getPatch().scene[scene],
                        storage.getPatch().scenedata[scene], key, velocity, channel, scene, detune,
                        &channelState[channel].keyState[key], &channelState[mpeMainChannel],
                        &channelState[channel], mpeEnabled, voiceCounter++);
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
}

void SurgeSynthesizer::releaseNote(char channel, char key, char velocity)
{
    bool foundVoice[n_scenes];
    for (int sc = 0; sc < n_scenes; ++sc)
    {
        foundVoice[sc] = false;
        for (auto *v : voices[sc])
        {
            foundVoice[sc] = true;
            if ((v->state.key == key) && (v->state.channel == channel))
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
             * RELEASE_IF_OTHERS_HELD mode we dont' want to hold it if we have other keys
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
            releaseNotePostHoldCheck(sc, channel, key, velocity);
        else
            holdbuffer[sc].push_back(
                std::make_pair(channel, key)); // hold pedal is down, add to bufffer
    }
}

void SurgeSynthesizer::releaseNotePostHoldCheck(int scene, char channel, char key, char velocity)
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

            switch (storage.getPatch().scene[v->state.scene_id].polymode.val.i)
            {
            case pm_poly:
                if ((v->state.key == key) && (v->state.channel == channel))
                    v->release();
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
                    int activateVoiceKey = 60, activateVoiceChannel = 0; // these will be overriden
                    auto priorityMode =
                        storage.getPatch().scene[v->state.scene_id].monoVoicePriorityMode;

                    // v->release();
                    if (!mpeEnabled)
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
                    else
                    {
                        int highest = -1, lowest = 128, latest = -1;
                        int hichan, lowchan, latechan;
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
                        // confirm that no notes are active
                        v->uber_release();
                        if (getNonUltrareleaseVoices(scene) == 0)
                        {
                            playVoice(scene, activateVoiceChannel, activateVoiceKey, velocity,
                                      channelState[activateVoiceChannel].keyState[k].lastdetune);
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
                    if (!mpeEnabled)
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
                    else
                    {
                        int highest = -1, lowest = 128, latest = -1;
                        int hichan, lowchan, latechan;
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

    if (lowest < 129)
        ((ControllerModulationSource *)storage.getPatch().scene[scene].modsources[ms_lowest_key])
            ->init((lowest - ktRoot) * twelfth);
    else if (resetToZeroOnLastRelease)
        ((ControllerModulationSource *)storage.getPatch().scene[scene].modsources[ms_lowest_key])
            ->init(0.f);

    if (highest >= 0)
        ((ControllerModulationSource *)storage.getPatch().scene[scene].modsources[ms_highest_key])
            ->init((highest - ktRoot) * twelfth);
    else if (resetToZeroOnLastRelease)
        ((ControllerModulationSource *)storage.getPatch().scene[scene].modsources[ms_highest_key])
            ->init(0.f);

    if (latest >= 0)
        ((ControllerModulationSource *)storage.getPatch().scene[scene].modsources[ms_latest_key])
            ->init((latest - ktRoot) * twelfth);
    else if (resetToZeroOnLastRelease)
        ((ControllerModulationSource *)storage.getPatch().scene[scene].modsources[ms_latest_key])
            ->init(0.f);
}

void SurgeSynthesizer::pitchBend(char channel, int value)
{
    if (mpeEnabled && channel != 0)
    {
        channelState[channel].pitchBend = value;

        /*
        ** TODO: handling of channel 0 and mpeGlobalPitchBendRange was broken with the addition
        ** of smoothing. we should probably add that back in if it turns out someone actually uses
        *it :)
        ** currently channelState[].pitchBendInSemitones is now unused, but it hasn't been removed
        *from
        ** the code yet for this reason.
        ** For now, we ignore channel zero here so it functions like the old code did in practice
        *when
        ** mpeGlobalPitchBendRange remained at zero.
        */
    }

    /*
    ** So here's the thing. We want global pitch bend modulation to work for other things in MPE
    *mode.
    ** This code has been here forever. But that means we need to ignore the channel[0] MPE
    *pitchbend
    ** elsewhere, especially since the range was hardwired to 2 (but is now 0). As far as I know the
    ** main MPE devices don't have a global pitch bend anyway so this just screws up regular
    *keyboards
    ** sending channel 0 pitch bend in MPE mode.
    */
    if (!mpeEnabled || channel == 0)
    {
        storage.pitch_bend = value / 8192.f;

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
    storage.poly_aftertouch[0][key & 127] = fval;
    storage.poly_aftertouch[1][key & 127] = fval;
}

void SurgeSynthesizer::programChange(char channel, int value)
{
    PCH = value;
    // load_patch((CC0<<7) + PCH);
    patchid_queue = (CC0 << 7) + PCH;
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
    /* OK there are two things we are dealing with here

     1: The MPE specification v1.0 section 2.1.1 says the message format for RPN is

        Bn 64 06
        Bn 65 00
        Bn 06 mm

     where n = 0 is lower zone, n=F is upper zone, all others are invalid, and mm=0 means no MPE and
     mm=1->F is zone.

     So you would think the Roli Seaboard would send, since it is one zone

        B0 64 06 B0 65 00 B0 06 0F

     and be done with it. If it did this code would work.

     But it doesn't. At least with Roli Seaboard Firmware 1.1.7.

     Instead on *each* channel it sends

        Bn 64 04 Bn 64 0 Bn 06 00
        Bn 64 03 Bn 64 0 Bn 06 00

     for each channel. Which seems unrelated to the spec. But as a result the original onRPN code
     means you get no MPE with a Roli Seaboard.

     Hey one year later an edit: Those aren't coming from ROLI they are coming from Logic PRO and
     now that I correct modify and stream MPE state, we should not listen to those messages.
     */

    if (lsbRPN == 0 && msbRPN == 0) // PITCH BEND RANGE
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
            storage.mpePitchBendRange = Surge::Storage::getUserDefaultValue(
                &storage, Surge::Storage::MPEPitchBendRange, 48);
        mpeGlobalPitchBendRange = 0;
        return;
    }
    else if (lsbRPN == 4 && msbRPN == 0 && channel != 0 && channel != 0xF)
    {
        /*
        ** This is code sent by logic in all cases for some reason. In ancient times
        ** I thought it came from a roli. But I since changed the MPE state management so
        ** with 1.6.5 do this:
        */
#if 0      
       // This is the invalid message which the ROLI sends. Rather than have the Roli not work
       mpeEnabled = true;
       mpeVoices = msbValue & 0xF;
       mpePitchBendRange = Surge::Storage::getUserDefaultValue(&storage, "mpePitchBendRange", 48);
       std::cout << __LINE__ << " " << __FILE__ << " MPEE=" << mpeEnabled << " MPEPBR=" << mpePitchBendRange << std::endl;
       mpeGlobalPitchBendRange = 0;
#endif
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

    return 0;
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

        channelState[channel].hold = value > 63; // check hold pedal

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
            channelState[channel].timbre = int7ToBipolarFloat(value);
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
    case 123: // all notes off
        return;
    };

    int cc_encoded = cc;

    if ((cc == 6) || (cc == 38)) // handle RPN/NRPNs (untested)
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

    for (int i = 0; i < n_customcontrollers; i++)
    {
        if (storage.controllers[i] == cc_encoded)
        {
            ((ControllerModulationSource *)storage.getPatch().scene[0].modsources[ms_ctrl1 + i])
                ->set_target01(fval);
        }
    }

    if (learn_param >= 0)
    {
        if (learn_param < n_global_params)
        {
            storage.getPatch().param_ptr[learn_param]->midictrl = cc_encoded;
        }
        else
        {
            int a = learn_param;
            if (learn_param >= (n_global_params + n_scene_params))
                a -= n_scene_params;

            storage.getPatch().param_ptr[a]->midictrl = cc_encoded;
            storage.getPatch().param_ptr[a + n_scene_params]->midictrl = cc_encoded;
        }
        learn_param = -1;
    }

    if ((learn_custom >= 0) && (learn_custom < n_customcontrollers))
    {
        storage.controllers[learn_custom] = cc_encoded;
        learn_custom = -1;
    }

    // if(storage.getPatch().scene_active.val.i == 1)
    for (int i = 0; i < n_global_params; i++)
    {
        if (storage.getPatch().param_ptr[i]->midictrl == cc_encoded)
        {
            this->setParameterSmoothed(i, fval);
            int j = 0;
            while (j < 7)
            {
                if ((refresh_ctrl_queue[j] > -1) && (refresh_ctrl_queue[j] != i))
                    j++;
                else
                    break;
            }
            refresh_ctrl_queue[j] = i;
            refresh_ctrl_queue_value[j] = fval;
        }
    }
    int a = n_global_params + storage.getPatch().scene_active.val.i * n_scene_params;
    for (int i = a; i < (a + n_scene_params); i++)
    {
        if (storage.getPatch().param_ptr[i]->midictrl == cc_encoded)
        {
            this->setParameterSmoothed(i, fval);
            int j = 0;
            while (j < 7)
            {
                if ((refresh_ctrl_queue[j] > -1) && (refresh_ctrl_queue[j] != i))
                    j++;
                else
                    break;
            }
            refresh_ctrl_queue[j] = i;
            refresh_ctrl_queue_value[j] = fval;
        }
    }
}

void SurgeSynthesizer::purgeHoldbuffer(int scene)
{
    std::list<std::pair<int, int>> retainBuffer;
    for (auto hp : holdbuffer[scene])
    {
        auto channel = hp.first;
        auto key = hp.second;

        if (channel < 0 || key < 0)
        {
            // std::cout << "Caught tricky double releease condition!" << std::endl;
        }
        else
        {
            if (!channelState[0].hold && !channelState[channel].hold)
            {
                releaseNotePostHoldCheck(scene, channel, key, 127);
            }
            else
            {
                retainBuffer.push_back(hp);
            }
        }
    }
    holdbuffer[scene] = retainBuffer;
}

void SurgeSynthesizer::allNotesOff()
{
    for (int i = 0; i < 16; i++)
    {
        channelState[i].hold = false;

        for (int k = 0; k < 128; k++)
        {
            channelState[i].keyState[k].keystate = 0;
            channelState[i].keyState[k].lastdetune = 0;
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

    hpA.suspend();
    hpB.suspend();

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
    sinus.set_rate(1000.0 * dsamplerate_inv);
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
        return &mControlInterpolator[Index];
    }

    Index = GetFreeControlInterpolatorIndex();

    if (Index >= 0)
    {
        // Add new
        mControlInterpolator[Index].id = Id;
        mControlInterpolatorUsed[Index] = true;

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

    if (index >= metaparam_offset)
    {
        ((ControllerModulationSource *)storage.getPatch().scene[0].modsources[ms_ctrl1 + index -
                                                                              metaparam_offset])
            ->set_target01(value, true);
        return false;
    }

    if (index < storage.getPatch().param_ptr.size())
    {
        pdata oldval;
        oldval.i = storage.getPatch().param_ptr[index]->val.i;

        storage.getPatch().param_ptr[index]->set_value_f01(value, force_integer);
        if (storage.getPatch().param_ptr[index]->affect_other_parameters)
        {
            storage.getPatch().update_controls();
            need_refresh = true;
        }

        /*if(storage.getPatch().param_ptr[index]->ctrltype == ct_polymode)
        {
                if (storage.getPatch().param_ptr[index]->val.i == pm_latch)
                {
                        int s = storage.getPatch().param_ptr[index]->scene - 1;
                        release_if_latched[s&1] = true;
                        release_anyway[s&1] = true;
                        // release old notes if previous polymode was latch
                }
        }*/

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
                // release old notes if previous polymode was latch
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
                 ** Wish there was a better way to figure out my osc but thsi works
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
        case ct_bool_fm:
        case ct_fbconfig:
            switch_toggled_queued = true;
            break;
        case ct_filtersubtype:
            // See above: We know the filter type for this subtype is at index - 1. Cap max to be
            // the fut-subtype
            {
                auto subp = storage.getPatch().param_ptr[index];
                auto filterType = storage.getPatch().param_ptr[index - 1]->val.i;
                auto maxIVal = fut_subcount[filterType];
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
                p->val.i = oldval.i; // so funnily we want to set the value *back* so the loadFX
                                     // picks up the change in fxsync
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
        bool got = false;
        for (int i = 0; i < 8; i++)
        {
            if (refresh_parameter_queue[i] < 0 || refresh_parameter_queue[i] == index)
            {
                refresh_parameter_queue[i] = index;
                got = true;
                break;
            }
        }
        if (!got)
            refresh_overflow = true;
    }
    return need_refresh;
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
    for (int s = 0; s < n_fx_slots; s++)
    {
        bool something_changed = false;
        if ((fxsync[s].type.val.i != storage.getPatch().fx[s].type.val.i) || force_reload_all)
        {
            fx_reload[s] = false;

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
                memcpy((void *)&storage.getPatch().fx[s].p, (void *)&fxsync[s].p,
                       sizeof(Parameter) * n_fx_params);

            // std::cout << "About to call reset with " << _D(initp) << " at " << s << " to " <<
            // fxsync[s].type.val.i << std::endl;
            std::lock_guard<std::mutex> g(fxSpawnMutex);

            fx[s].reset(spawn_effect(storage.getPatch().fx[s].type.val.i, &storage,
                                     &storage.getPatch().fx[s], storage.getPatch().globaldata));
            if (fx[s])
            {
                fx[s]->init_ctrltypes();
                if (initp)
                    fx[s]->init_default_values();
                else
                {
                    for (int j = 0; j < n_fx_params; j++)
                    {
                        auto p = &(storage.getPatch().fx[s].p[j]);
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
                            clearModulation(p->id, (modsources)ms, true);
                        }
                    }
                    if (fx_reload_mod[s])
                    {
                        for (auto &t : fxmodsync[s])
                        {
                            setModulation(storage.getPatch().fx[s].p[std::get<1>(t)].id,
                                          (modsources)std::get<0>(t), std::get<2>(t));
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
                        clearModulation(p->id, (modsources)ms, true);
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
            if (storage.getPatch().fx[s].type.val.i != fxt_off)
                memcpy((void *)&storage.getPatch().fx[s].p, (void *)&fxsync[s].p,
                       sizeof(Parameter) * n_fx_params);
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

    // if (something_changed) storage.getPatch().update_controls(false);
    return true;
}

bool SurgeSynthesizer::loadOscalgos()
{
    for (int s = 0; s < n_scenes; s++)
    {
        for (int i = 0; i < n_oscs; i++)
        {
            bool resend = false;

            if (storage.getPatch().scene[s].osc[i].queue_type > -1)
            {
                // clear assigned modulation if we change osc type, see issue #2224
                if (storage.getPatch().scene[s].osc[i].queue_type !=
                    storage.getPatch().scene[s].osc[i].type.val.i)
                {
                    clear_osc_modulation(s, i);
                }

                storage.getPatch().scene[s].osc[i].type.val.i =
                    storage.getPatch().scene[s].osc[i].queue_type;
                storage.getPatch().update_controls(false, &storage.getPatch().scene[s].osc[i]);
                storage.getPatch().scene[s].osc[i].queue_type = -1;
                switch_toggled_queued = true;
                refresh_editor = true;
                resend = true;
            }

            TiXmlElement *e = (TiXmlElement *)storage.getPatch().scene[s].osc[i].queue_xmldata;
            if (e)
            {
                resend = true;
                for (int k = 0; k < n_osc_params; k++)
                {
                    double d;
                    int j;
                    char lbl[TXT_SIZE];

                    snprintf(lbl, TXT_SIZE, "p%i", k);

                    if (storage.getPatch().scene[s].osc[i].p[k].valtype == vt_float)
                    {
                        if (e->QueryDoubleAttribute(lbl, &d) == TIXML_SUCCESS)
                        {
                            storage.getPatch().scene[s].osc[i].p[k].val.f = (float)d;
                        }
                    }
                    else
                    {
                        if (e->QueryIntAttribute(lbl, &j) == TIXML_SUCCESS)
                        {
                            storage.getPatch().scene[s].osc[i].p[k].val.i = j;
                        }
                    }

                    snprintf(lbl, TXT_SIZE, "p%i_deform_type", k);

                    if (e->QueryIntAttribute(lbl, &j) == TIXML_SUCCESS)
                    {
                        storage.getPatch().scene[s].osc[i].p[k].deform_type = j;
                    }

                    snprintf(lbl, TXT_SIZE, "p%i_extend_range", k);

                    if (e->QueryIntAttribute(lbl, &j) == TIXML_SUCCESS)
                    {
                        storage.getPatch().scene[s].osc[i].p[k].extend_range = j;
                    }
                }

                int rt;
                if (e->QueryIntAttribute("retrigger", &rt) == TIXML_SUCCESS)
                {
                    storage.getPatch().scene[s].osc[i].retrigger.val.b = rt;
                }

                /*
                 * Some oscillator types can change display when you change values
                 */
                if (storage.getPatch().scene[s].osc[i].type.val.i == ot_modern)
                {
                    refresh_editor = true;
                }
                storage.getPatch().scene[s].osc[i].queue_xmldata = 0;
            }
        }
    }
    return true;
}

bool SurgeSynthesizer::isValidModulation(long ptag, modsources modsource)
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

ModulationRouting *SurgeSynthesizer::getModRouting(long ptag, modsources modsource)
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
        if ((modlist->at(i).destination_id == id) && (modlist->at(i).source_id == modsource))
        {
            return &modlist->at(i);
        }
    }

    return nullptr;
}

float SurgeSynthesizer::getModDepth(long ptag, modsources modsource)
{
    ModulationRouting *r = getModRouting(ptag, modsource);
    float d = 0.f;
    if (r)
        d = r->depth;
    Parameter *p = storage.getPatch().param_ptr.at(ptag);
    if (p && p->extend_range)
        d = p->get_extended(d);
    return d;
}

bool SurgeSynthesizer::isActiveModulation(long ptag, modsources modsource)
{
    // if(!isValidModulation(ptag,modsource)) return false;
    if (getModRouting(ptag, modsource))
        return true;
    return false;
}

bool SurgeSynthesizer::isBipolarModulation(modsources tms)
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

bool SurgeSynthesizer::isModDestUsed(long ptag)
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
            vector<ModulationRouting> *modlist;

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
            assert((id > 0) && (id < n_modsources));
            modsourceused[id] = true;
        }
    }
}

void SurgeSynthesizer::prepareModsourceDoProcess(int scenemask)
{
    for (int scene = 0; scene < n_scenes; scene++)
    {
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

float SurgeSynthesizer::getModulation(long ptag, modsources modsource)
{
    if (!isValidModulation(ptag, modsource))
        return 0.0f;

    ModulationRouting *r = getModRouting(ptag, modsource);
    if (r)
        return storage.getPatch().param_ptr[ptag]->get_modulation_f01(r->depth);

    return storage.getPatch().param_ptr[ptag]->get_modulation_f01(0);
}

bool SurgeSynthesizer::isModulationMuted(long ptag, modsources modsource)
{
    if (!isValidModulation(ptag, modsource))
        return false;

    ModulationRouting *r = getModRouting(ptag, modsource);
    if (r)
        return r->muted;

    return false;
}

void SurgeSynthesizer::muteModulation(long ptag, modsources modsource, bool mute)
{
    if (!isValidModulation(ptag, modsource))
        return;

    ModulationRouting *r = getModRouting(ptag, modsource);
    if (r)
        r->muted = mute;
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

void SurgeSynthesizer::clearModulation(long ptag, modsources modsource, bool clearEvenIfInvalid)
{
    if (!isValidModulation(ptag, modsource) && !clearEvenIfInvalid)
        return;

    int scene = storage.getPatch().param_ptr[ptag]->scene;

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

    int id = storage.getPatch().param_ptr[ptag]->param_id_in_scene;
    if (!scene)
        id = ptag;
    int n = modlist->size();

    for (int i = 0; i < n; i++)
    {
        if ((modlist->at(i).destination_id == id) && (modlist->at(i).source_id == modsource))
        {
            storage.modRoutingMutex.lock();
            modlist->erase(modlist->begin() + i);
            storage.modRoutingMutex.unlock();
            return;
        }
    }
}

bool SurgeSynthesizer::setModulation(long ptag, modsources modsource, float val)
{
    if (!isValidModulation(ptag, modsource))
        return false;
    float value = storage.getPatch().param_ptr[ptag]->set_modulation_f01(val);
    int scene = storage.getPatch().param_ptr[ptag]->scene;

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
        if ((modlist->at(i).destination_id == id) && (modlist->at(i).source_id == modsource))
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
            modlist->push_back(t);
        }
        else
        {
            modlist->at(found_id).depth = value;
        }
    }
    storage.modRoutingMutex.unlock();

    return true;
}

float SurgeSynthesizer::getParameter01(long index)
{
    if (index < 0)
        return 0.f;
    if (index >= metaparam_offset)
        return storage.getPatch()
            .scene[0]
            .modsources[ms_ctrl1 + index - metaparam_offset]
            ->get_output01();
    if (index < storage.getPatch().param_ptr.size())
        return storage.getPatch().param_ptr[index]->get_value_f01();
    return 0.f;
}

void SurgeSynthesizer::getParameterDisplay(long index, char *text)
{
    if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
    {
        storage.getPatch().param_ptr[index]->get_display(text);
    }
    else if (index >= metaparam_offset)
    {
        snprintf(text, TXT_SIZE, "%.2f %%",
                 100.f * storage.getPatch()
                             .scene[0]
                             .modsources[ms_ctrl1 + index - metaparam_offset]
                             ->get_output());
    }
    else
        snprintf(text, TXT_SIZE, "-");
}

void SurgeSynthesizer::getParameterDisplayAlt(long index, char *text)
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

void SurgeSynthesizer::getParameterDisplay(long index, char *text, float x)
{
    if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
    {
        storage.getPatch().param_ptr[index]->get_display(text, true, x);
    }
    else if (index >= metaparam_offset)
    {
        snprintf(text, TXT_SIZE, "%.2f %%",
                 100.f * storage.getPatch()
                             .scene[0]
                             .modsources[ms_ctrl1 + index - metaparam_offset]
                             ->get_output());
    }
    else
        snprintf(text, TXT_SIZE, "-");
}

void SurgeSynthesizer::getParameterName(long index, char *text)
{
    if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
    {
        int scn = storage.getPatch().param_ptr[index]->scene;
        // TODO: FIX SCENE ASSUMPTION
        string sn[3] = {"", "A ", "B "};

        snprintf(text, TXT_SIZE, "%s%s", sn[scn].c_str(),
                 storage.getPatch().param_ptr[index]->get_full_name());
    }
    else if (index >= metaparam_offset)
    {
        int c = index - metaparam_offset;
        snprintf(text, TXT_SIZE, "Macro %i: %s", c + 1,
                 storage.getPatch().CustomControllerLabel[c]);
    }
    else
        snprintf(text, TXT_SIZE, "-");
}

void SurgeSynthesizer::getParameterNameW(long index, wchar_t *ptr)
{
    if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
    {
        int scn = storage.getPatch().param_ptr[index]->scene;
        // TODO: FIX SCENE ASSUMPTION
        char sn[3][3] = {"", "A ", "B "};
        char pname[256];

        snprintf(pname, 255, "%s%s", sn[scn], storage.getPatch().param_ptr[index]->get_full_name());

        // the input is not wide so don't use %S
        swprintf(ptr, 128, L"%s", pname);
    }
    else if (index >= metaparam_offset)
    {
        int c = index - metaparam_offset;
        // For a reason I don't understand, on windows, we need to sprintf then swprinf just the
        // short char to make just these names work. :shrug:
        char wideHack[256];

        if (c >= num_metaparameters)
        {
            snprintf(wideHack, 255, "Macro: ERROR");
        }
        else
        {
            snprintf(wideHack, 255, "Macro %d: %s", c + 1,
                     storage.getPatch().CustomControllerLabel[c]);
        }
        swprintf(ptr, 128, L"%s", wideHack);
    }
    else
    {
        swprintf(ptr, 128, L"-");
    }
}

void SurgeSynthesizer::getParameterShortNameW(long index, wchar_t *ptr)
{
    if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
    {
        int scn = storage.getPatch().param_ptr[index]->scene;
        // TODO: FIX SCENE ASSUMPTION
        string sn[3] = {"", "A ", "B "};

        swprintf(ptr, 128, L"%s%s", sn[scn].c_str(),
                 storage.getPatch().param_ptr[index]->get_name());
    }
    else if (index >= metaparam_offset)
    {
        getParameterNameW(index, ptr);
    }
    else
    {
        swprintf(ptr, 128, L"-");
    }
}

void SurgeSynthesizer::getParameterUnitW(long index, wchar_t *ptr)
{
    if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
    {
        swprintf(ptr, 128, L"%s", storage.getPatch().param_ptr[index]->getUnit());
    }
    else
    {
        swprintf(ptr, 128, L"");
    }
}

void SurgeSynthesizer::getParameterStringW(long index, float value, wchar_t *ptr)
{
    if ((index >= 0) && (index < storage.getPatch().param_ptr.size()))
    {
        char text[128];
        storage.getPatch().param_ptr[index]->get_display(text, true, value);

        swprintf(ptr, 128, L"%s", text);
    }
    else if (index >= metaparam_offset)
    {
        // For a reason I don't understand, on windows, we need to sprintf then swprinf just the
        // short char to make just these names work. :shrug:
        char wideHack[256];
        snprintf(wideHack, 256, "%.2f %%", 100.f * value);
        swprintf(ptr, 128, L"%s", wideHack);
    }
    else
    {
        swprintf(ptr, 128, L"-");
    }
}

void SurgeSynthesizer::getParameterMeta(long index, parametermeta &pm)
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
        pm.hide = (pm.flags & kHide) != 0;
        pm.meta = (pm.flags & kMeta) != 0;
        pm.expert = !(pm.flags & kEasy);
        pm.clump = 2;
        if (storage.getPatch().param_ptr[index]->scene)
        {
            pm.clump += storage.getPatch().param_ptr[index]->ctrlgroup +
                        (storage.getPatch().param_ptr[index]->scene - 1) * 6;
            if (storage.getPatch().param_ptr[index]->ctrlgroup == 0)
                pm.clump++;
        }
    }
    else if (index >= metaparam_offset)
    {
        pm.flags = 0;
        pm.fmin = 0.f;
        pm.fmax = 1.f;
        pm.fdefault = 0.5f;
        pm.hide = false;
        pm.meta =
            false; // ironic as they are metaparameters, but they don't affect any other sliders
        pm.expert = false;
        pm.clump = 1;
    }
}
/*unsigned int sub3_synth::getParameterFlags (long index)
{
        if (index<storage.getPatch().param_ptr.size())
        {
                return storage.getPatch().param_ptr[index]->ctrlstyle;
        }
        return 0;
}*/

float SurgeSynthesizer::getParameter(long index)
{
    if (index < 0)
        return 0.f;
    if (index >= metaparam_offset)
        return storage.getPatch()
            .scene[0]
            .modsources[ms_ctrl1 + index - metaparam_offset]
            ->get_output();
    if (index < storage.getPatch().param_ptr.size())
        return storage.getPatch().param_ptr[index]->get_value_f01();
    return 0.f;
}

float SurgeSynthesizer::normalizedToValue(long index, float value)
{
    if (index < 0)
        return 0.f;
    if (index >= metaparam_offset)
        return value;
    if (index < storage.getPatch().param_ptr.size())
        return storage.getPatch().param_ptr[index]->normalized_to_value(value);
    return 0.f;
}

float SurgeSynthesizer::valueToNormalized(long index, float value)
{
    if (index < 0)
        return 0.f;
    if (index >= metaparam_offset)
        return value;
    if (index < storage.getPatch().param_ptr.size())
        return storage.getPatch().param_ptr[index]->value_to_normalized(value);
    return 0.f;
}

bool SurgeSynthesizer::stringToNormalizedValue(const ID &index, std::string s, float &outval)
{
    int id = index.getSynthSideId();
    if (id < 0)
        return false;
    if (id >= metaparam_offset)
        return false;
    if (id >= storage.getPatch().param_ptr.size())
        return false;

    auto p = storage.getPatch().param_ptr[id];
    if (p->valtype != vt_float)
        return false;

    pdata vt = p->val;
    auto res = p->set_value_from_string_onto(s, vt);

    if (res)
    {
        outval = (vt.f - p->val_min.f) / (p->val_max.f - p->val_min.f);
        return true;
    }

    return false;
}

#if MAC || LINUX
void *loadPatchInBackgroundThread(void *sy)
{
#else
DWORD WINAPI loadPatchInBackgroundThread(LPVOID lpParam)
{
    void *sy = lpParam;
#endif

    SurgeSynthesizer *synth = (SurgeSynthesizer *)sy;
    std::lock_guard<std::mutex> mg(synth->patchLoadSpawnMutex);
    if (synth->patchid_queue >= 0)
    {
        int patchid = synth->patchid_queue;
        synth->patchid_queue = -1;
        synth->allNotesOff();
        synth->loadPatch(patchid);
    }
    if (synth->has_patchid_file)
    {
        auto p(string_to_path(synth->patchid_file));
        auto s = path_to_string(p.stem());
        synth->has_patchid_file = false;
        synth->allNotesOff();

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
            synth->loadPatchByPath(synth->patchid_file, -1, s.c_str());
        }
    }

    synth->halt_engine = false;

    return 0;
}

void SurgeSynthesizer::processThreadunsafeOperations(bool dangerMode)
{
    if (!audio_processing_active || dangerMode)
    {
        // if the audio processing is inactive, patchloading should occur anyway
        if (patchid_queue >= 0)
        {
            loadPatch(patchid_queue);
            patchid_queue = -1;
        }

        if (load_fx_needed)
            loadFx(false, false);

        loadOscalgos();
    }
}

void SurgeSynthesizer::resetStateFromTimeData()
{
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

    // TODO: FIX SCENE ASSUMPTION
    if (playA && (storage.getPatch().scene[0].polymode.val.i == pm_latch) && voices[0].empty())
        playNote(1, 60, 100, 0);
    if (playB && (storage.getPatch().scene[1].polymode.val.i == pm_latch) && voices[1].empty())
        playNote(2, 60, 100, 0);

    // interpolate MIDI controllers
    for (int i = 0; i < num_controlinterpolators; i++)
    {
        if (mControlInterpolatorUsed[i])
        {
            ControllerModulationSource *mc = &mControlInterpolator[i];
            bool cont = mc->process_block_until_close(0.001f);
            int id = mc->id;
            storage.getPatch().param_ptr[id]->set_value_f01(mc->output);
            if (!cont)
            {
                mControlInterpolatorUsed[i] = false;
            }
        }
    }

    storage.getPatch().copy_globaldata(
        storage.getPatch()
            .globaldata); // Drains a great deal of CPU while in Debug mode.. optimize?

    // TODO: FIX SCENE ASSUMPTION
    if (playA)
        storage.getPatch().copy_scenedata(storage.getPatch().scenedata[0], 0); // -""-
    if (playB)
        storage.getPatch().copy_scenedata(storage.getPatch().scenedata[1], 1);

    //	if(sm == sm_morph) storage.getPatch().do_morph();

    prepareModsourceDoProcess((playA ? 1 : 0) | (playB ? 2 : 0));

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
                if (storage.getPatch().scene[s].modsources[src_id])
                {
                    int dst_id = storage.getPatch().scene[s].modulation_scene[i].destination_id;
                    float depth = storage.getPatch().scene[s].modulation_scene[i].depth;
                    storage.getPatch().scenedata[s][dst_id].f +=
                        depth * storage.getPatch().scene[s].modsources[src_id]->output *
                        (1.0 - storage.getPatch().scene[s].modulation_scene[i].muted);
                }
            }

            for (int i = 0; i < n_lfos_scene; i++)
                storage.getPatch().scene[s].modsources[ms_slfo1 + i]->process_block();
        }
    }

    loadOscalgos();

    int n = storage.getPatch().modulation_global.size();
    for (int i = 0; i < n; i++)
    {
        int src_id = storage.getPatch().modulation_global[i].source_id;
        int dst_id = storage.getPatch().modulation_global[i].destination_id;
        float depth = storage.getPatch().modulation_global[i].depth;
        storage.getPatch().globaldata[dst_id].f +=
            depth * storage.getPatch().scene[0].modsources[src_id]->output *
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

    if (storage.oddsound_mts_client)
    {
        storage.oddsound_mts_on_check = (storage.oddsound_mts_on_check + 1) & (1024 - 1);
        if (storage.oddsound_mts_on_check == 0)
        {
            bool prior = storage.oddsound_mts_active;
            storage.oddsound_mts_active = MTS_HasMaster(storage.oddsound_mts_client);
            if (prior != storage.oddsound_mts_active)
            {
                refresh_editor = true;
            }
        }
    }
}

void SurgeSynthesizer::process()
{
#if DEBUG_RNG_THREADING
    storage.audioThreadID = pthread_self();
#endif

    float mfade = 1.f;

    if (halt_engine)
    {
        clear_block(output[0], BLOCK_SIZE_QUAD);
        clear_block(output[1], BLOCK_SIZE_QUAD);
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
            allNotesOff();
            halt_engine = true;

#if MAC || LINUX
            pthread_t thread;
            pthread_attr_t attributes;
            int ret;
            sched_param params;

            /* initialized with default attributes */
            ret = pthread_attr_init(&attributes);

            /* safe to get existing scheduling param */
            ret = pthread_attr_getschedparam(&attributes, &params);

            /* set the priority; others are unchanged */
            params.sched_priority = sched_get_priority_min(SCHED_OTHER);

            /* setting the new scheduling param */
            ret = pthread_attr_setschedparam(&attributes, &params);

            ret = pthread_create(&thread, &attributes, loadPatchInBackgroundThread, this);
#else

            DWORD dwThreadId;
            HANDLE hThread = CreateThread(NULL, // default security attributes
                                          0,    // use default stack size
                                          loadPatchInBackgroundThread, // thread function
                                          this,         // argument to thread function
                                          0,            // use default creation flags
                                          &dwThreadId); // returns the thread identifier
            SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);
#endif

            clear_block(output[0], BLOCK_SIZE_QUAD);
            clear_block(output[1], BLOCK_SIZE_QUAD);
            return;
        }
    }

    // process inputs (upsample & halfrate)
    if (process_input)
    {
        hardclip_block8(input[0], BLOCK_SIZE_QUAD);
        hardclip_block8(input[1], BLOCK_SIZE_QUAD);
        copy_block(input[0], storage.audio_in_nonOS[0], BLOCK_SIZE_QUAD);
        copy_block(input[1], storage.audio_in_nonOS[1], BLOCK_SIZE_QUAD);
        halfbandIN.process_block_U2(input[0], input[1], storage.audio_in[0], storage.audio_in[1]);
    }
    else
    {
        clear_block_antidenormalnoise(storage.audio_in[0], BLOCK_SIZE_OS_QUAD);
        clear_block_antidenormalnoise(storage.audio_in[1], BLOCK_SIZE_OS_QUAD);
        clear_block_antidenormalnoise(storage.audio_in_nonOS[1], BLOCK_SIZE_QUAD);
        clear_block_antidenormalnoise(storage.audio_in_nonOS[1], BLOCK_SIZE_QUAD);
    }

    // TODO: FIX SCENE ASSUMPTION
    float fxsendout alignas(16)[2][2][BLOCK_SIZE];
    bool play_scene[n_scenes];

    {
        clear_block_antidenormalnoise(sceneout[0][0], BLOCK_SIZE_OS_QUAD);
        clear_block_antidenormalnoise(sceneout[0][1], BLOCK_SIZE_OS_QUAD);
        clear_block_antidenormalnoise(sceneout[1][0], BLOCK_SIZE_OS_QUAD);
        clear_block_antidenormalnoise(sceneout[1][1], BLOCK_SIZE_OS_QUAD);

        clear_block_antidenormalnoise(fxsendout[0][0], BLOCK_SIZE_QUAD);
        clear_block_antidenormalnoise(fxsendout[0][1], BLOCK_SIZE_QUAD);
        clear_block_antidenormalnoise(fxsendout[1][0], BLOCK_SIZE_QUAD);
        clear_block_antidenormalnoise(fxsendout[1][1], BLOCK_SIZE_QUAD);
    }

    storage.modRoutingMutex.lock();
    processControl();

    amp.set_target_smoothed(db_to_linear(storage.getPatch().volume.val.f));
    amp_mute.set_target(mfade);

    int fx_bypass = storage.getPatch().fx_bypass.val.i;

    if (fx_bypass == fxb_all_fx)
    {
        if (fx[fxslot_send1])
        {
            FX1.set_target_smoothed(
                amp_to_linear(storage.getPatch()
                                  .globaldata[storage.getPatch().fx[fxslot_send1].return_level.id]
                                  .f));
            send[0][0].set_target_smoothed(amp_to_linear(
                storage.getPatch()
                    .scenedata[0][storage.getPatch().scene[0].send_level[0].param_id_in_scene]
                    .f));
            send[0][1].set_target_smoothed(amp_to_linear(
                storage.getPatch()
                    .scenedata[1][storage.getPatch().scene[1].send_level[0].param_id_in_scene]
                    .f));
        }
        if (fx[fxslot_send2])
        {
            FX2.set_target_smoothed(
                amp_to_linear(storage.getPatch()
                                  .globaldata[storage.getPatch().fx[fxslot_send2].return_level.id]
                                  .f));
            send[1][0].set_target_smoothed(amp_to_linear(
                storage.getPatch()
                    .scenedata[0][storage.getPatch().scene[0].send_level[1].param_id_in_scene]
                    .f));
            send[1][1].set_target_smoothed(amp_to_linear(
                storage.getPatch()
                    .scenedata[1][storage.getPatch().scene[1].send_level[1].param_id_in_scene]
                    .f));
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

        fbq_global g;
        g.FU1ptr = GetQFPtrFilterUnit(storage.getPatch().scene[s].filterunit[0].type.val.i,
                                      storage.getPatch().scene[s].filterunit[0].subtype.val.i);
        g.FU2ptr = GetQFPtrFilterUnit(storage.getPatch().scene[s].filterunit[1].type.val.i,
                                      storage.getPatch().scene[s].filterunit[1].subtype.val.i);
        g.WSptr = GetQFPtrWaveshaper(storage.getPatch().scene[s].wsunit.type.val.i);

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
            copy_block(sceneout[0][0], storage.audio_otherscene[0], BLOCK_SIZE_OS_QUAD);
            copy_block(sceneout[0][1], storage.audio_otherscene[1], BLOCK_SIZE_OS_QUAD);
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
    }

    storage.modRoutingMutex.unlock();
    polydisplay = vcount;

    // TODO: FIX SCENE ASSUMPTION
    if (play_scene[0])
    {
        switch (storage.sceneHardclipMode[0])
        {
        case SurgeStorage::HARDCLIP_TO_18DBFS:
            hardclip_block8(sceneout[0][0], BLOCK_SIZE_OS_QUAD);
            hardclip_block8(sceneout[0][1], BLOCK_SIZE_OS_QUAD);
            break;
        case SurgeStorage::HARDCLIP_TO_0DBFS:
            hardclip_block(sceneout[0][0], BLOCK_SIZE_OS_QUAD);
            hardclip_block(sceneout[0][1], BLOCK_SIZE_OS_QUAD);
            break;
        case SurgeStorage::BYPASS_HARDCLIP:
            break;
        }

        halfbandA.process_block_D2(sceneout[0][0], sceneout[0][1]);
    }

    if (play_scene[1])
    {
        switch (storage.sceneHardclipMode[1])
        {
        case SurgeStorage::HARDCLIP_TO_18DBFS:
            hardclip_block8(sceneout[1][0], BLOCK_SIZE_OS_QUAD);
            hardclip_block8(sceneout[1][1], BLOCK_SIZE_OS_QUAD);
            break;
        case SurgeStorage::HARDCLIP_TO_0DBFS:
            hardclip_block(sceneout[1][0], BLOCK_SIZE_OS_QUAD);
            hardclip_block(sceneout[1][1], BLOCK_SIZE_OS_QUAD);
            break;
        case SurgeStorage::BYPASS_HARDCLIP:
            break;
        }

        halfbandB.process_block_D2(sceneout[1][0], sceneout[1][1]);
    }

    // TODO: FIX SCENE ASSUMPTION
    if (storage.getPatch().scene[0].lowcut.deactivated == false)
    {
        auto freq =
            storage.getPatch().scenedata[0][storage.getPatch().scene[0].lowcut.param_id_in_scene].f;

        hpA.coeff_HP(hpA.calc_omega(freq / 12.0), 0.4);    // var 0.707
        hpA.process_block(sceneout[0][0], sceneout[0][1]); // TODO: quadify
    }

    if (storage.getPatch().scene[1].lowcut.deactivated == false)
    {
        auto freq =
            storage.getPatch().scenedata[1][storage.getPatch().scene[1].lowcut.param_id_in_scene].f;

        hpB.coeff_HP(hpB.calc_omega(freq / 12.0), 0.4);
        hpB.process_block(sceneout[1][0], sceneout[1][1]);
    }

    for (int cls = 0; cls < n_scenes; ++cls)
    {
        switch (storage.sceneHardclipMode[cls])
        {
        case SurgeStorage::HARDCLIP_TO_18DBFS:
            hardclip_block8(sceneout[cls][0], BLOCK_SIZE_QUAD);
            hardclip_block8(sceneout[cls][1], BLOCK_SIZE_QUAD);
            break;
        case SurgeStorage::HARDCLIP_TO_0DBFS:
            hardclip_block(sceneout[cls][0], BLOCK_SIZE_QUAD);
            hardclip_block(sceneout[cls][1], BLOCK_SIZE_QUAD);
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

    // apply insert effects
    if (fx_bypass != fxb_no_fx)
    {
        if (fx[fxslot_ains1] && !(storage.getPatch().fx_disable.val.i & (1 << 0)))
        {
            sc_state[0] =
                fx[fxslot_ains1]->process_ringout(sceneout[0][0], sceneout[0][1], sc_state[0]);
        }

        if (fx[fxslot_ains2] && !(storage.getPatch().fx_disable.val.i & (1 << 1)))
        {
            sc_state[0] =
                fx[fxslot_ains2]->process_ringout(sceneout[0][0], sceneout[0][1], sc_state[0]);
        }

        if (fx[fxslot_bins1] && !(storage.getPatch().fx_disable.val.i & (1 << 2)))
        {
            sc_state[1] =
                fx[fxslot_bins1]->process_ringout(sceneout[1][0], sceneout[1][1], sc_state[1]);
        }

        if (fx[fxslot_bins2] && !(storage.getPatch().fx_disable.val.i & (1 << 3)))
        {
            sc_state[1] =
                fx[fxslot_bins2]->process_ringout(sceneout[1][0], sceneout[1][1], sc_state[1]);
        }
    }

    for (int cls = 0; cls < n_scenes; ++cls)
    {
        switch (storage.sceneHardclipMode[cls])
        {
        case SurgeStorage::HARDCLIP_TO_18DBFS:
            hardclip_block8(sceneout[cls][0], BLOCK_SIZE_QUAD);
            hardclip_block8(sceneout[cls][1], BLOCK_SIZE_QUAD);
            break;
        case SurgeStorage::HARDCLIP_TO_0DBFS:
            hardclip_block(sceneout[cls][0], BLOCK_SIZE_QUAD);
            hardclip_block(sceneout[cls][1], BLOCK_SIZE_QUAD);
            break;
        default:
            break;
        }
    }

    // sum scenes
    // TODO: FIX SCENE ASSUMPTION
    copy_block(sceneout[0][0], output[0], BLOCK_SIZE_QUAD);
    copy_block(sceneout[0][1], output[1], BLOCK_SIZE_QUAD);
    accumulate_block(sceneout[1][0], output[0], BLOCK_SIZE_QUAD);
    accumulate_block(sceneout[1][1], output[1], BLOCK_SIZE_QUAD);

    bool send1 = false, send2 = false;
    // add send effects
    // TODO: FIX SCENE ASSUMPTION
    if (fx_bypass == fxb_all_fx)
    {
        if (fx[fxslot_send1] && !(storage.getPatch().fx_disable.val.i & (1 << 4)))
        {
            send[0][0].MAC_2_blocks_to(sceneout[0][0], sceneout[0][1], fxsendout[0][0],
                                       fxsendout[0][1], BLOCK_SIZE_QUAD);
            send[0][1].MAC_2_blocks_to(sceneout[1][0], sceneout[1][1], fxsendout[0][0],
                                       fxsendout[0][1], BLOCK_SIZE_QUAD);
            send1 = fx[fxslot_send1]->process_ringout(fxsendout[0][0], fxsendout[0][1],
                                                      sc_state[0] || sc_state[1]);
            FX1.MAC_2_blocks_to(fxsendout[0][0], fxsendout[0][1], output[0], output[1],
                                BLOCK_SIZE_QUAD);
        }
        if (fx[fxslot_send2] && !(storage.getPatch().fx_disable.val.i & (1 << 5)))
        {
            send[1][0].MAC_2_blocks_to(sceneout[0][0], sceneout[0][1], fxsendout[1][0],
                                       fxsendout[1][1], BLOCK_SIZE_QUAD);
            send[1][1].MAC_2_blocks_to(sceneout[1][0], sceneout[1][1], fxsendout[1][0],
                                       fxsendout[1][1], BLOCK_SIZE_QUAD);
            send2 = fx[fxslot_send2]->process_ringout(fxsendout[1][0], fxsendout[1][1],
                                                      sc_state[0] || sc_state[1]);
            FX2.MAC_2_blocks_to(fxsendout[1][0], fxsendout[1][1], output[0], output[1],
                                BLOCK_SIZE_QUAD);
        }
    }

    // apply global effects
    if ((fx_bypass == fxb_all_fx) || (fx_bypass == fxb_no_sends))
    {
        bool glob = sc_state[0] || sc_state[1] || send1 || send2;

        if (fx[fxslot_global1] && !(storage.getPatch().fx_disable.val.i & (1 << 6)))
        {
            glob = fx[fxslot_global1]->process_ringout(output[0], output[1], glob);
        }

        if (fx[fxslot_global2] && !(storage.getPatch().fx_disable.val.i & (1 << 7)))
        {
            glob = fx[fxslot_global2]->process_ringout(output[0], output[1], glob);
        }
    }

    amp.multiply_2_blocks(output[0], output[1], BLOCK_SIZE_QUAD);
    amp_mute.multiply_2_blocks(output[0], output[1], BLOCK_SIZE_QUAD);

    // VU
    // falloff
    float a = storage.vu_falloff;
    vu_peak[0] = min(2.f, a * vu_peak[0]);
    vu_peak[1] = min(2.f, a * vu_peak[1]);
    vu_peak[0] = max(vu_peak[0], get_absmax(output[0], BLOCK_SIZE_QUAD));
    vu_peak[1] = max(vu_peak[1], get_absmax(output[1], BLOCK_SIZE_QUAD));

    switch (storage.hardclipMode)
    {
    case SurgeStorage::HARDCLIP_TO_18DBFS:
        hardclip_block8(output[0], BLOCK_SIZE_QUAD);
        hardclip_block8(output[1], BLOCK_SIZE_QUAD);
        break;

    case SurgeStorage::HARDCLIP_TO_0DBFS:
        hardclip_block(output[0], BLOCK_SIZE_QUAD);
        hardclip_block(output[1], BLOCK_SIZE_QUAD);
        break;
    case SurgeStorage::BYPASS_HARDCLIP:
        break;
    }

    // since the sceneout is now routable we also need to mute it
    for (int sc = 0; sc < n_scenes; ++sc)
    {
        amp_mute.multiply_2_blocks(sceneout[sc][0], sceneout[sc][1], BLOCK_SIZE_QUAD);
    }
}

SurgeSynthesizer::PluginLayer *SurgeSynthesizer::getParent()
{
    assert(_parent != nullptr);
    return _parent;
}

void SurgeSynthesizer::populateDawExtraState()
{
    storage.getPatch().dawExtraState.isPopulated = true;
    storage.getPatch().dawExtraState.mpeEnabled = mpeEnabled;
    storage.getPatch().dawExtraState.mpePitchBendRange = storage.mpePitchBendRange;

    storage.getPatch().dawExtraState.hasScale = !storage.isStandardScale;
    if (!storage.isStandardScale)
        storage.getPatch().dawExtraState.scaleContents = storage.currentScale.rawText;
    else
        storage.getPatch().dawExtraState.scaleContents = "";

    storage.getPatch().dawExtraState.hasMapping = !storage.isStandardMapping;
    if (!storage.isStandardMapping)
    {
        storage.getPatch().dawExtraState.mappingContents = storage.currentMapping.rawText;
        storage.getPatch().dawExtraState.mappingName = storage.currentMapping.name;
    }
    else
    {
        storage.getPatch().dawExtraState.mappingContents = "";
        storage.getPatch().dawExtraState.mappingName = "";
    }

    int n = n_global_params + n_scene_params; // only store midictrl's for scene A (scene A -> scene
                                              // B will be duplicated on load)
    for (int i = 0; i < n; i++)
    {
        if (storage.getPatch().param_ptr[i]->midictrl >= 0)
        {
            storage.getPatch().dawExtraState.midictrl_map[i] =
                storage.getPatch().param_ptr[i]->midictrl;
        }
    }

    for (int i = 0; i < n_customcontrollers; ++i)
    {
        storage.getPatch().dawExtraState.customcontrol_map[i] = storage.controllers[i];
    }

    storage.getPatch().dawExtraState.monoPedalMode = storage.monoPedalMode;
    storage.getPatch().dawExtraState.oddsoundRetuneMode = storage.oddsoundRetuneMode;
}

void SurgeSynthesizer::loadFromDawExtraState()
{
    if (!storage.getPatch().dawExtraState.isPopulated)
        return;
    mpeEnabled = storage.getPatch().dawExtraState.mpeEnabled;
    if (storage.getPatch().dawExtraState.mpePitchBendRange > 0)
        storage.mpePitchBendRange = storage.getPatch().dawExtraState.mpePitchBendRange;

    storage.monoPedalMode = (MonoPedalMode)storage.getPatch().dawExtraState.monoPedalMode;
    storage.oddsoundRetuneMode =
        (SurgeStorage::OddsoundRetuneMode)storage.getPatch().dawExtraState.oddsoundRetuneMode;

    if (storage.getPatch().dawExtraState.hasScale)
    {
        try
        {
            auto sc = Tunings::parseSCLData(storage.getPatch().dawExtraState.scaleContents);
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

    if (storage.getPatch().dawExtraState.hasMapping)
    {
        try
        {
            auto kb = Tunings::parseKBMData(storage.getPatch().dawExtraState.mappingContents);
            if (storage.getPatch().dawExtraState.mappingName.size() > 1)
            {
                kb.name = storage.getPatch().dawExtraState.mappingName;
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

    int n = n_global_params + n_scene_params; // only store midictrl's for scene A (scene A -> scene
                                              // B will be duplicated on load)
    for (int i = 0; i < n; i++)
    {
        if (storage.getPatch().dawExtraState.midictrl_map.find(i) !=
            storage.getPatch().dawExtraState.midictrl_map.end())
        {
            storage.getPatch().param_ptr[i]->midictrl =
                storage.getPatch().dawExtraState.midictrl_map[i];
            if (i >= n_global_params)
            {
                storage.getPatch().param_ptr[i + n_scene_params]->midictrl =
                    storage.getPatch().dawExtraState.midictrl_map[i];
            }
        }
    }

    for (int i = 0; i < n_customcontrollers; ++i)
    {
        if (storage.getPatch().dawExtraState.customcontrol_map.find(i) !=
            storage.getPatch().dawExtraState.customcontrol_map.end())
            storage.controllers[i] = storage.getPatch().dawExtraState.customcontrol_map[i];
    }
}

void SurgeSynthesizer::setupActivateExtraOutputs()
{
    bool defval = true;
    if (hostProgram.find("Fruit") == 0) // FruityLoops default off
        defval = false;

    activateExtraOutputs = Surge::Storage::getUserDefaultValue(
        &(storage), Surge::Storage::ActivateExtraOutputs, defval ? 1 : 0);
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
                int n = mv->size();
                for (int i = 0; i < n; ++i)
                {
                    if (mv->at(i).source_id == ms_ctrl1 + c1)
                    {
                        auto q = mv->at(i);
                        q.source_id = ms_ctrl1 + c2;
                        mv->at(i) = q;
                    }
                    else if (mv->at(i).source_id == ms_ctrl1 + c2)
                    {
                        auto q = mv->at(i);
                        q.source_id = ms_ctrl1 + c1;
                        mv->at(i) = q;
                    }
                }
            }
        }
    }

    storage.modRoutingMutex.unlock();

    refresh_editor = true;
}

void SurgeSynthesizer::changeModulatorSmoothing(ControllerModulationSource::SmoothingMode m)
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
    if (source < 0 || source >= n_fx_slots || target < 0 || target >= n_fx_slots)
        return;

    std::lock_guard<std::recursive_mutex> lockModulation(storage.modRoutingMutex);

    FxStorage so, to;
    memcpy((void *)&so, (void *)(&storage.getPatch().fx[source]), sizeof(FxStorage));
    memcpy((void *)&to, (void *)(&storage.getPatch().fx[target]), sizeof(FxStorage));

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

    if (m == FXReorderMode::MOVE)
    {
        fxsync[source].type.val.i = 0;
    }
    else if (m == FXReorderMode::SWAP)
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
    // else leave the source alone

    /*
     * OK we can't copy the params - they contain things like id in scene - we need to copy the
     * values
     */
    auto cp = [](Parameter &to, const Parameter &from) {
        to.val = from.val;
        to.temposync = from.temposync;
        to.extend_range = from.extend_range;
        to.deactivated = from.deactivated;
        to.absolute = from.absolute;
    };
    for (int i = 0; i < n_fx_params; ++i)
    {
        if (m == FXReorderMode::SWAP)
            cp(fxsync[source].p[i], to.p[i]);
        cp(fxsync[target].p[i], so.p[i]);
    }

    // Now swap the routings. FX routings are always global
    std::vector<ModulationRouting> *mv = nullptr;
    mv = &(storage.getPatch().modulation_global);
    int n = mv->size();
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
                getModulation(fxsync[source].p[whichForReal].id, (modsources)mv->at(i).source_id);
            fxmodsync[target].push_back(std::make_tuple(mv->at(i).source_id, whichForReal, depth));
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
                auto depth = getModulation(fxsync[target].p[whichForReal].id,
                                           (modsources)mv->at(i).source_id);
                fxmodsync[source].push_back(
                    std::make_tuple(mv->at(i).source_id, whichForReal, depth));
            }
        }
    }

    load_fx_needed = true;
    fx_reload[source] = true;
    fx_reload[target] = true;
    refresh_editor = true;
}

bool SurgeSynthesizer::getParameterIsBoolean(const ID &id)
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
