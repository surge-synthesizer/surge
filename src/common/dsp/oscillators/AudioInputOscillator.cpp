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

#include "AudioInputOscillator.h"

AudioInputOscillator::AudioInputOscillator(SurgeStorage *storage, OscillatorStorage *oscdata,
                                           pdata *localcopy)
    : Oscillator(storage, oscdata, localcopy), lp(storage), hp(storage)
{
    // in case of more scenes being added, design a solution for audio in oscillator extra input!
    isInSceneB = false;
    if (storage)
    {
        storage->otherscene_clients++;
        bool isSB = false;
        for (int i = 0; i < n_oscs; ++i)
            if (&(storage->getPatch().scene[1].osc[i]) == oscdata)
                isSB = true;

        isInSceneB = isSB;
    }
}

void AudioInputOscillator::init(float pitch, bool is_display, bool nonzero_init_drift)
{
    hp.coeff_instantize();
    lp.coeff_instantize();

    hp.coeff_HP(hp.calc_omega(oscdata->p[audioin_lowcut].val.f / 12.0) / OSC_OVERSAMPLING, 0.707);
    lp.coeff_LP2B(lp.calc_omega(oscdata->p[audioin_highcut].val.f / 12.0) / OSC_OVERSAMPLING,
                  0.707);
}

AudioInputOscillator::~AudioInputOscillator()
{
    if (storage)
        storage->otherscene_clients--;
}

void AudioInputOscillator::init_ctrltypes(int scene, int osc)
{
    oscdata->p[audioin_channel].set_name("Audio In Channel");
    oscdata->p[audioin_channel].set_type(ct_percent_bipolar_stereo);

    oscdata->p[audioin_gain].set_name("Audio In Gain");
    oscdata->p[audioin_gain].set_type(ct_decibel);

    if (scene == 1)
    {
        oscdata->p[audioin_sceneAchan].set_name("Scene A Channel");
        oscdata->p[audioin_sceneAchan].set_type(ct_percent_bipolar_stereo);

        oscdata->p[audioin_sceneAgain].set_name("Scene A Gain");
        oscdata->p[audioin_sceneAgain].set_type(ct_decibel);

        oscdata->p[audioin_sceneAmix].set_name("Scene A Mix");
        oscdata->p[audioin_sceneAmix].set_type(ct_percent);
    }
    oscdata->p[audioin_lowcut].set_name("Low Cut");
    oscdata->p[audioin_lowcut].set_type(ct_freq_audible_deactivatable_hp);

    oscdata->p[audioin_highcut].set_name("High Cut");
    oscdata->p[audioin_highcut].set_type(ct_freq_audible_deactivatable_lp);
}

void AudioInputOscillator::init_default_values()
{
    oscdata->p[audioin_channel].val.f = 0.0f;
    oscdata->p[audioin_gain].val.f = 0.0f;

    if (isInSceneB)
    {
        oscdata->p[audioin_sceneAchan].val.f = 0.0f;
        oscdata->p[audioin_sceneAgain].val.f = 0.0f;
        oscdata->p[audioin_sceneAmix].val.f = 0.0f;
    }

    // low cut at the bottom
    oscdata->p[audioin_lowcut].val.f = oscdata->p[audioin_lowcut].val_min.f;
    oscdata->p[audioin_lowcut].deactivated = true;

    // high cut at the top
    oscdata->p[audioin_highcut].val.f = oscdata->p[audioin_highcut].val_max.f;
    oscdata->p[audioin_highcut].deactivated = true;
}

void AudioInputOscillator::process_block(float pitch, float drift, bool stereo, bool FM,
                                         float FMdepth)
{
    bool useOtherScene = false;
    if (isInSceneB && localcopy[oscdata->p[audioin_sceneAmix].param_id_in_scene].f > 0.f)
    {
        useOtherScene = true;
    }

    float inGain = storage->db_to_linear(localcopy[oscdata->p[audioin_gain].param_id_in_scene].f);
    float inChMix =
        limit_range(localcopy[oscdata->p[audioin_channel].param_id_in_scene].f, -1.f, 1.f);
    float sceneGain =
        storage->db_to_linear(localcopy[oscdata->p[audioin_sceneAgain].param_id_in_scene].f);
    float sceneChMix =
        limit_range(localcopy[oscdata->p[audioin_sceneAchan].param_id_in_scene].f, -1.f, 1.f);
    float sceneMix = localcopy[oscdata->p[audioin_sceneAmix].param_id_in_scene].f;
    float inverseMix = 1.f - sceneMix;

    float l = inGain * (1.f - inChMix);
    float r = inGain * (1.f + inChMix);

    float sl = sceneGain * (1.f - sceneChMix);
    float sr = sceneGain * (1.f + sceneChMix);

    if (stereo)
    {
        for (int k = 0; k < BLOCK_SIZE_OS; k++)
        {
            if (useOtherScene)
            {
                output[k] = (l * storage->audio_in[0][k] * inverseMix) +
                            (sl * storage->audio_otherscene[0][k] * sceneMix);
                outputR[k] = (r * storage->audio_in[1][k] * inverseMix) +
                             (sr * storage->audio_otherscene[1][k] * sceneMix);
            }
            else
            {
                output[k] = l * storage->audio_in[0][k];
                outputR[k] = r * storage->audio_in[1][k];
            }
        }
    }
    else
    {
        for (int k = 0; k < BLOCK_SIZE_OS; k++)
        {
            if (useOtherScene)
            {
                output[k] =
                    (((l * storage->audio_in[0][k]) + (r * storage->audio_in[1][k])) * inverseMix) +
                    (((sl * storage->audio_otherscene[0][k]) +
                      (sr * storage->audio_otherscene[1][k])) *
                     sceneMix);
            }
            else
            {
                output[k] = l * storage->audio_in[0][k] + r * storage->audio_in[1][k];
            }
        }
    }

    applyFilter();
}

void AudioInputOscillator::applyFilter()
{
    if (!oscdata->p[audioin_lowcut].deactivated)
    {
        auto par = &(oscdata->p[audioin_lowcut]);
        auto pv = limit_range(localcopy[par->param_id_in_scene].f, par->val_min.f, par->val_max.f);
        hp.coeff_HP(hp.calc_omega(pv / 12.0) / OSC_OVERSAMPLING, 0.707);
    }

    if (!oscdata->p[audioin_highcut].deactivated)
    {
        auto par = &(oscdata->p[audioin_highcut]);
        auto pv = limit_range(localcopy[par->param_id_in_scene].f, par->val_min.f, par->val_max.f);
        lp.coeff_LP2B(lp.calc_omega(pv / 12.0) / OSC_OVERSAMPLING, 0.707);
    }

    for (int k = 0; k < BLOCK_SIZE_OS; k += BLOCK_SIZE)
    {
        if (!oscdata->p[audioin_lowcut].deactivated)
            hp.process_block(&(output[k]), &(outputR[k]));
        if (!oscdata->p[audioin_highcut].deactivated)
            lp.process_block(&(output[k]), &(outputR[k]));
    }
}

void AudioInputOscillator::handleStreamingMismatches(int streamingRevision,
                                                     int currentSynthStreamingRevision)
{
    if (streamingRevision <= 12)
    {
        oscdata->p[audioin_lowcut].val.f =
            oscdata->p[audioin_lowcut].val_min.f; // low cut at the bottom
        oscdata->p[audioin_lowcut].deactivated = true;
        oscdata->p[audioin_highcut].val.f =
            oscdata->p[audioin_highcut].val_max.f; // high cut at the top
        oscdata->p[audioin_highcut].deactivated = true;
    }
}
