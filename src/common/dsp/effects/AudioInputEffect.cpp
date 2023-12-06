/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2023, various authors, as described in the GitHub
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

#include "AudioInputEffect.h"

AudioInputEffect::AudioInputEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd)
{
    effect_slot_type slotType = getSlotType(fxdata->fxslot);

    if (storage && (slotType == a_insert_slot || slotType == b_insert_slot))
    {
        int scene = slotType == a_insert_slot ? 1 : 0;

        for (int i = 0; i < N_OUTPUTS; i++)
        {
            sceneDataPtr[i] = storage->scenesOutputData.getSceneData(scene, i);
        }
    }
}

void AudioInputEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    //   audio input
    fxdata->p[in_audio_input_channel].set_name("Channel");
    fxdata->p[in_audio_input_channel].set_type(ct_percent_bipolar_stereo);
    fxdata->p[in_audio_input_channel].posy_offset = 1;

    fxdata->p[in_audio_input_pan].set_name("Pan");
    fxdata->p[in_audio_input_pan].set_type(ct_percent_bipolar_stereo);
    fxdata->p[in_audio_input_pan].posy_offset = 1;

    fxdata->p[in_audio_input_level].set_name("Level");
    fxdata->p[in_audio_input_level].set_type(ct_decibel_attenuation_large);
    fxdata->p[in_audio_input_level].posy_offset = 1;

    // effect input
    fxdata->p[in_effect_input_channel].set_name("Channel");
    fxdata->p[in_effect_input_channel].set_type(ct_percent_bipolar_stereo);
    fxdata->p[in_effect_input_channel].posy_offset = 3;

    fxdata->p[in_effect_input_pan].set_name("Pan");
    fxdata->p[in_effect_input_pan].set_type(ct_percent_bipolar_stereo);
    fxdata->p[in_effect_input_pan].posy_offset = 3;

    fxdata->p[in_effect_input_level].set_name("Level");
    fxdata->p[in_effect_input_level].set_type(ct_decibel_attenuation_large);
    fxdata->p[in_effect_input_level].posy_offset = 3;

    // scene input
    effect_slot_type slot_type = getSlotType(fxdata->fxslot);

    if (slot_type == a_insert_slot || slot_type == b_insert_slot)
    {
        fxdata->p[in_scene_input_channel].set_name("Channel");
        fxdata->p[in_scene_input_channel].set_type(ct_percent_bipolar_stereo);
        fxdata->p[in_scene_input_channel].posy_offset = 5;

        fxdata->p[in_scene_input_pan].set_name("Pan");
        fxdata->p[in_scene_input_pan].set_type(ct_percent_bipolar_stereo);
        fxdata->p[in_scene_input_pan].posy_offset = 5;

        fxdata->p[in_scene_input_level].set_name("Level");
        fxdata->p[in_scene_input_level].set_type(ct_decibel_attenuation_large);
        fxdata->p[in_scene_input_level].posy_offset = 5;
    }

    // output
    fxdata->p[in_output_width].set_name("Width");
    fxdata->p[in_output_width].set_type(ct_percent_bipolar);
    fxdata->p[in_output_width].posy_offset = 7;

    fxdata->p[in_output_mix].set_name("Mix");
    fxdata->p[in_output_mix].set_type(ct_percent);
    fxdata->p[in_output_mix].posy_offset = 7;
}

void AudioInputEffect::init_default_values()
{
    fxdata->p[in_audio_input_channel].val.f = 0.0;
    fxdata->p[in_audio_input_pan].val.f = 0.0;
    fxdata->p[in_audio_input_level].val.f = -96.0;

    fxdata->p[in_effect_input_channel].val.f = 0.0;
    fxdata->p[in_effect_input_pan].val.f = 0.0;
    fxdata->p[in_effect_input_level].val.f = 0.0;

    fxdata->p[in_scene_input_channel].val.f = 0.0;
    fxdata->p[in_scene_input_pan].val.f = 0.0;
    fxdata->p[in_scene_input_level].val.f = -96.0;

    fxdata->p[in_output_width].val.f = 1.0;
    fxdata->p[in_output_mix].val.f = 1.0;
}

const char *AudioInputEffect::group_label(int id)
{
    std::vector group_labels = {{"Audio Input", "Effect Input", "Scene Input", "Output"}};
    effect_slot_type slot_type = getSlotType(fxdata->fxslot);

    if (slot_type == a_insert_slot)
    {
        group_labels[2] = "Scene B Input";
    }
    else if (slot_type == b_insert_slot)
    {
        group_labels[2] = "Scene A Input";
    }
    else
    {
        group_labels.erase(group_labels.begin() + 2);
    }

    if (id >= 0 && id < group_labels.size())
    {
        return group_labels[id];
    }

    return 0;
}

int AudioInputEffect::group_label_ypos(int id)
{
    std::vector ypos = {1, 9, 17, 25};
    effect_slot_type slot_type = getSlotType(fxdata->fxslot);

    if (slot_type != a_insert_slot && slot_type != b_insert_slot)
    {
        ypos.erase(ypos.begin() + 2);
    }

    if (id >= 0 && id < ypos.size())
    {
        return ypos[id];
    }

    return 0;
}

AudioInputEffect::effect_slot_type AudioInputEffect::getSlotType(fxslot_positions p)
{
    switch (p)
    {
    case fxslot_ains1:
    case fxslot_ains2:
    case fxslot_ains3:
    case fxslot_ains4:
        return AudioInputEffect::a_insert_slot;
    case fxslot_bins1:
    case fxslot_bins2:
    case fxslot_bins3:
    case fxslot_bins4:
        return AudioInputEffect::b_insert_slot;
    case fxslot_send1:
    case fxslot_send2:
    case fxslot_send3:
    case fxslot_send4:
        return AudioInputEffect::send_slot;
    default:
        return AudioInputEffect::global_slot;
    }
}

void AudioInputEffect::process(float *dataL, float *dataR)
{
    auto &effectInputChannel = *pd_float[in_effect_input_channel];
    auto &effectInputPan = *pd_float[in_effect_input_pan];
    auto &effectInputLevel = *pd_float[in_effect_input_level];
    float *drySignal[] = {dataL, dataR};
    float effectDataBuffer[2][BLOCK_SIZE];

    std::memcpy(effectDataBuffer[0], dataL, BLOCK_SIZE * sizeof(float));
    std::memcpy(effectDataBuffer[1], dataR, BLOCK_SIZE * sizeof(float));

    float *effectDataBuffers[]{effectDataBuffer[0], effectDataBuffer[1]};

    applyParameters(effectDataBuffers, effectInputChannel, effectInputPan, effectInputLevel);

    auto &inputChannel = *pd_float[in_audio_input_channel];
    auto &inputPan = *pd_float[in_audio_input_pan];
    auto &inputLevel = *pd_float[in_audio_input_level];
    float *inputData[] = {storage->audio_in_nonOS[0], storage->audio_in_nonOS[1]};
    float inputDataBuffer[2][BLOCK_SIZE];

    std::memcpy(inputDataBuffer[0], inputData[0], BLOCK_SIZE * sizeof(float));
    std::memcpy(inputDataBuffer[1], inputData[1], BLOCK_SIZE * sizeof(float));

    float *inputDataBuffers[] = {inputDataBuffer[0], inputDataBuffer[1]};

    applyParameters(inputDataBuffers, inputChannel, inputPan, inputLevel);

    effect_slot_type slotType = getSlotType(fxdata->fxslot);

    if (slotType == a_insert_slot || slotType == b_insert_slot)
    {
        auto &sceneInputChannel = *pd_float[in_scene_input_channel];
        auto &sceneInputPan = *pd_float[in_scene_input_pan];
        auto &sceneInputLevel = *pd_float[in_scene_input_level];

        float *sceneData[] = {
            sceneDataPtr[0].get(),
            sceneDataPtr[1].get(),
        };

        float sceneDataBuffer[2][BLOCK_SIZE];

        std::memcpy(sceneDataBuffer[0], sceneData[0], BLOCK_SIZE * sizeof(float));
        std::memcpy(sceneDataBuffer[1], sceneData[1], BLOCK_SIZE * sizeof(float));

        float *sceneDataBuffers[]{sceneDataBuffer[0], sceneDataBuffer[1]};

        applyParameters(sceneDataBuffers, sceneInputChannel, sceneInputPan, sceneInputLevel);

        // mixing the scene audio input and the effect audio input
        for (int i = 0; i < BLOCK_SIZE; ++i)
        {
            effectDataBuffer[0][i] += sceneDataBuffer[0][i];
            effectDataBuffer[1][i] += sceneDataBuffer[1][i];
        }
    }

    // mixing the effect and audio input
    for (int i = 0; i < BLOCK_SIZE; ++i)
    {
        effectDataBuffer[0][i] += inputDataBuffer[0][i];
        effectDataBuffer[1][i] += inputDataBuffer[1][i];
    }

    auto &outputWidth = *pd_float[in_output_width];
    auto &outputMix = *pd_float[in_output_mix];

    float *dryL = drySignal[0];
    float *dryR = drySignal[1];
    float *wetL = effectDataBuffer[0];
    float *wetR = effectDataBuffer[1];

    // Adjust width of wet signal
    for (int i = 0; i < BLOCK_SIZE; ++i)
    {
        float mid = 0.5f * (wetL[i] + wetR[i]);                // Mid (mono) signal
        float side = outputWidth * 0.5f * (wetL[i] - wetR[i]); // Sides (stereo) signal

        wetL[i] = mid + side;
        wetR[i] = mid - side;
    }

    // Mix dry and wet signal according to outputMix
    for (int i = 0; i < BLOCK_SIZE; ++i)
    {
        float wetMix = outputMix;
        float dryMix = 1.0f - outputMix;

        dryL[i] = dryMix * dryL[i] + wetMix * wetL[i];
        dryR[i] = dryMix * dryR[i] + wetMix * wetR[i];
    }
}

void AudioInputEffect::applyParameters(float *buffer[], const float &channel, const float &pan,
                                       const float &level)
{
    float leftGain, rightGain;

    if (channel < 0)
    {
        leftGain = 1.0f;
        rightGain = 1.0f + channel;
    }
    else
    {
        leftGain = 1.0f - channel;
        rightGain = 1.0f;
    }

    for (int i = 0; i < BLOCK_SIZE; ++i)
    {
        buffer[0][i] *= leftGain;
        buffer[1][i] *= rightGain;
    }

    float tempBuffer[2][BLOCK_SIZE];

    std::memcpy(tempBuffer[0], buffer[0], BLOCK_SIZE * sizeof(float));
    std::memcpy(tempBuffer[1], buffer[1], BLOCK_SIZE * sizeof(float));

    float leftToLeft = (pan < 0) ? 1.0f : 1.0f - pan;
    float rightToRight = (pan > 0) ? 1.0f : 1.0f + pan;

    for (int i = 0; i < BLOCK_SIZE; ++i)
    {
        buffer[0][i] = buffer[0][i] * leftToLeft + tempBuffer[1][i] * (1.0f - rightToRight);
        buffer[1][i] = buffer[1][i] * rightToRight + tempBuffer[0][i] * (1.0f - leftToLeft);
    }

    float effectInputLevelGain = std::pow(10.0f, level / 20.0f);

    for (int i = 0; i < BLOCK_SIZE; ++i)
    {
        buffer[0][i] *= effectInputLevelGain;
        buffer[1][i] *= effectInputLevelGain;
    }
}