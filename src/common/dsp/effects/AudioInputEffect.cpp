
#include "AudioInputEffect.h"
#include "juce_audio_basics/juce_audio_basics.h"
AudioInputEffect::AudioInputEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd) {}
AudioInputEffect::~AudioInputEffect() {}
void AudioInputEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    // -----  Audio Input
    fxdata->p[in_audio_input_channel].set_name("Channel");
    fxdata->p[in_audio_input_channel].set_type(ct_percent_bipolar_stereo);
    fxdata->p[in_audio_input_channel].posy_offset = 1;

    fxdata->p[in_audio_input_pan].set_name("Pan");
    fxdata->p[in_audio_input_pan].set_type(ct_percent_bipolar_stereo);
    fxdata->p[in_audio_input_pan].posy_offset = 1;

    fxdata->p[in_audio_input_level].set_name("Level");
    fxdata->p[in_audio_input_level].set_type(ct_decibel_attenuation);
    fxdata->p[in_audio_input_level].posy_offset = 1;

    // -----  Effect Input
    fxdata->p[in_effect_input_channel].set_name("Channel");
    fxdata->p[in_effect_input_channel].set_type(ct_percent_bipolar_stereo);
    fxdata->p[in_effect_input_channel].posy_offset = 3;

    fxdata->p[in_effect_input_pan].set_name("Pan");
    fxdata->p[in_effect_input_pan].set_type(ct_percent_bipolar_stereo);
    fxdata->p[in_effect_input_pan].posy_offset = 3;

    fxdata->p[in_effect_input_level].set_name("Level");
    fxdata->p[in_effect_input_level].set_type(ct_decibel_attenuation);
    fxdata->p[in_effect_input_level].posy_offset = 3;

    // -----  Scene Input
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
        fxdata->p[in_scene_input_level].set_type(ct_decibel_attenuation);
        fxdata->p[in_scene_input_level].posy_offset = 5;

    }

    // -----  Output
    fxdata->p[in_output_width].set_name("Width");
    fxdata->p[in_output_width].set_type(ct_percent_bipolar_stereo);
    fxdata->p[in_output_width].posy_offset = 7;

    fxdata->p[in_output_mix].set_name("Mix");
    fxdata->p[in_output_mix].set_type(ct_percent);
    fxdata->p[in_output_mix].posy_offset = 7;

}

void AudioInputEffect::init_default_values()
{
    fxdata->p[in_audio_input_channel].val.f = 0.0; // p0
    fxdata->p[in_audio_input_pan].val.f = 0.0;   //p1
    fxdata->p[in_audio_input_level].val.f = -48.0;  //p2


    fxdata->p[in_effect_input_channel].val.f = 0.0; //p3
    fxdata->p[in_effect_input_pan].val.f = 0.0;  //p4
    fxdata->p[in_effect_input_level].val.f = 1.0; //p5


    fxdata->p[in_scene_input_channel].val.f = 0.0; //p6
    fxdata->p[in_scene_input_pan].val.f = 0.0; //p7
    fxdata->p[in_scene_input_level].val.f = 0.0; //p8

    fxdata->p[in_output_width].val.f = 0.0; //p9
    fxdata->p[in_output_mix].val.f = 1.0;  //p10
}
const char *AudioInputEffect::group_label(int id) {
    std::vector group_labels = {{"Audio Input", "Effect Input", "Scene Input", "Output"}};
    effect_slot_type slot_type = getSlotType(fxdata->fxslot);
    if (slot_type == a_insert_slot)
        group_labels[2] = "Scene B Input";
    else if (slot_type == b_insert_slot)
        group_labels[2] = "Scene A Input";
    else
        group_labels.erase(group_labels.begin() + 2);
    if (id>=0 && id < group_labels.size())
        return group_labels[id];
    return 0;
}


int AudioInputEffect::group_label_ypos(int id) {
    std::vector ypos = {1, 9, 17, 25};
    effect_slot_type slot_type = getSlotType(fxdata->fxslot);
    if (slot_type != a_insert_slot && slot_type != b_insert_slot)
        ypos.erase(ypos.begin() + 2);
    if (id>=0 && id < ypos.size())
        return ypos[id];
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
    //So... let's see what we have here...
    //We have 3 inputs, and 1 output
    // input 1 is the audio input
    // input 2 is the effect input, but only if we're in an insert slot
    // input 3 is the scene input
    // we only need to take left or right channel with. But how shall we interpret channel if it is a value between 0 and 1?
    // 0.0 = left
    // 0.5 = center
    // 1.0 = right
    // but what is 0.4? 0.4 is 40% of the way from left to center. So we need to take 40% of the left channel and 60% of the right channel
    // output is the mix of all 3 inputs

    float& audioInputChannel = fxdata->p[in_audio_input_channel].val.f;
    float leftGain, rightGain;

    if (audioInputChannel < 0) {
            leftGain = 1.0f;
            rightGain = 1.0f + audioInputChannel; // When audioInputChannel is -1, rightGain will be 0
    } else {
            leftGain = 1.0f - audioInputChannel; // When audioInputChannel is 1, leftGain will be 0
            rightGain = 1.0f;
    }
//    float* channelData[] = { dataL, dataR };
//    juce::AudioBuffer<float> buffer(channelData, 2, BLOCK_SIZE);

    // Apply gains to the dataL and dataR arrays
    for (int i = 0; i < BLOCK_SIZE; ++i) {
            dataL[i] *= leftGain;
            dataR[i] *= rightGain;
    }


}