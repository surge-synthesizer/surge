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

#if !defined(_M_ARM64EC)
#include "BBDEnsembleEffect.h"
#endif

#include "BonsaiEffect.h"
#include "ChorusEffectImpl.h"
#include "CombulatorEffect.h"
#include "ConditionerEffect.h"
#include "DistortionEffect.h"
#include "DelayEffect.h"
#include "FlangerEffect.h"
#include "FrequencyShifterEffect.h"
#include "GraphicEQ11BandEffect.h"
#include "MSToolEffect.h"
#include "NimbusEffect.h"
#include "ParametricEQ3BandEffect.h"
#include "PhaserEffect.h"
#include "ResonatorEffect.h"
#include "Reverb1Effect.h"
#include "Reverb2Effect.h"
#include "RingModulatorEffect.h"
#include "RotarySpeakerEffect.h"
#include "TreemonsterEffect.h"
#include "VocoderEffect.h"
#include "WaveShaperEffect.h"
#include "airwindows/AirWindowsEffect.h"
#include "chowdsp/NeuronEffect.h"
#include "chowdsp/CHOWEffect.h"
#include "chowdsp/ExciterEffect.h"
#include "chowdsp/SpringReverbEffect.h"
#include "chowdsp/TapeEffect.h"
#include "DebugHelpers.h"
#include "AudioInputEffect.h"
#include "FloatyDelayEffect.h"

using namespace std;

Effect *spawn_effect(int id, SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
{
    // std::cout << "Spawn Effect " << _D(id) << std::endl;
    // Surge::Debug::stackTraceToStdout(7);
    switch (id)
    {
    case fxt_delay:
        return new DelayEffect(storage, fxdata, pd);
    case fxt_eq:
        return new ParametricEQ3BandEffect(storage, fxdata, pd);
    case fxt_phaser:
        return new PhaserEffect(storage, fxdata, pd);
    case fxt_rotaryspeaker:
        return new RotarySpeakerEffect(storage, fxdata, pd);
    case fxt_distortion:
        return new DistortionEffect(storage, fxdata, pd);
    case fxt_reverb:
        return new Reverb1Effect(storage, fxdata, pd);
    case fxt_reverb2:
        return new Reverb2Effect(storage, fxdata, pd);
    case fxt_freqshift:
        return new FrequencyShifterEffect(storage, fxdata, pd);
    case fxt_conditioner:
        return new ConditionerEffect(storage, fxdata, pd);
    case fxt_chorus4:
        return new ChorusEffect<4>(storage, fxdata, pd);
    case fxt_vocoder:
        return new VocoderEffect(storage, fxdata, pd);
    case fxt_flanger:
        return new FlangerEffect(storage, fxdata, pd);
    case fxt_ringmod:
        return new RingModulatorEffect(storage, fxdata, pd);
    case fxt_airwindows:
        return new AirWindowsEffect(storage, fxdata, pd);
    case fxt_neuron:
        return new chowdsp::NeuronEffect(storage, fxdata, pd);
    case fxt_geq11:
        return new GraphicEQ11BandEffect(storage, fxdata, pd);
    case fxt_resonator:
        return new ResonatorEffect(storage, fxdata, pd);
    case fxt_combulator:
        return new CombulatorEffect(storage, fxdata, pd);
    case fxt_chow:
        return new chowdsp::CHOWEffect(storage, fxdata, pd);
    case fxt_nimbus:
        return new NimbusEffect(storage, fxdata, pd);
    case fxt_exciter:
        return new chowdsp::ExciterEffect(storage, fxdata, pd);
    case fxt_tape:
        return new chowdsp::TapeEffect(storage, fxdata, pd);
    case fxt_ensemble:
#if defined(_M_ARM64EC)
        return nullptr;
#else
        return new BBDEnsembleEffect(storage, fxdata, pd);
#endif
    case fxt_treemonster:
        return new TreemonsterEffect(storage, fxdata, pd);
    case fxt_waveshaper:
        return new WaveShaperEffect(storage, fxdata, pd);
    case fxt_mstool:
        return new MSToolEffect(storage, fxdata, pd);
    case fxt_spring_reverb:
        return new chowdsp::SpringReverbEffect(storage, fxdata, pd);
    case fxt_bonsai:
        return new BonsaiEffect(storage, fxdata, pd);
    case fxt_audio_input:
        return new AudioInputEffect(storage, fxdata, pd);
    case fxt_floaty_delay:
        return new FloatyDelayEffect(storage, fxdata, pd);

    default:
        return 0;
    };
}

Effect::Effect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
{
    // assert(storage);
    this->fxdata = fxdata;
    this->storage = storage;
    this->pd = pd;
    ringout = 10000000;
    if (pd)
    {
        for (int i = 0; i < n_fx_params; i++)
        {
            pd_float[i] = &pd[fxdata->p[i].id].f;
            pd_int[i] = &pd[fxdata->p[i].id].i;
        }
    }
}

bool Effect::process_ringout(float *dataL, float *dataR, bool indata_present)
{
    if (indata_present)
        ringout = 0;
    else
        ringout++;

    int d = get_ringout_decay();
    if ((d < 0) || (ringout < d) || (ringout == 0))
    {
        process(dataL, dataR);
        return true;
    }
    else
        process_only_control();
    return false;
}

void Effect::init_ctrltypes()
{
    for (int j = 0; j < n_fx_params; j++)
    {
        fxdata->p[j].modulateable = true;
        fxdata->p[j].set_type(ct_none);
    }
}
