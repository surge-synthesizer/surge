
#include "AudioInputEffect.h"

AudioInputEffect::AudioInputEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd) {
    effect_slot_type slotType = getSlotType(fxdata->fxslot);
    //TODO: next step is taking care of cases when we move the effect to another slot
    if (storage && (slotType== a_insert_slot || slotType == b_insert_slot))
    {
        int scene = slotType == a_insert_slot ? 1 : 0;
        storage->scenesOutputData.increaseNumberOfClients(scene);
    }
}
AudioInputEffect::~AudioInputEffect() {
    effect_slot_type slotType = getSlotType(fxdata->fxslot);
    if (storage && (slotType== a_insert_slot || slotType == b_insert_slot))
    {
        int scene = slotType == a_insert_slot ? 1 : 0;
        storage->scenesOutputData.decreaseNumberOfClients(scene);
    }
}
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
    float& effectInputChannel = fxdata->p[in_effect_input_channel].val.f;
    float& effectInputPan = fxdata->p[in_effect_input_pan].val.f;
    float& effectInputLevelDb = fxdata->p[in_effect_input_level].val.f;
    float* channelData[] = { dataL, dataR };

    juce::AudioBuffer<float> effectDataBuffer(channelData, 2, BLOCK_SIZE);
    applySlidersControls(effectDataBuffer, effectInputChannel, effectInputPan, effectInputLevelDb);


    float& inputChannel = fxdata->p[in_audio_input_channel].val.f;
    float& inputPan = fxdata->p[in_audio_input_pan].val.f;
    float& inputLevelDb = fxdata->p[in_audio_input_level].val.f;
    float* inputData[] = { storage->audio_in_nonOS[0], storage->audio_in_nonOS[1] };
    juce::AudioBuffer<float> inputDataBuffer( 2, BLOCK_SIZE);
    inputDataBuffer.copyFrom(0, 0, inputData[0], BLOCK_SIZE);
    inputDataBuffer.copyFrom(1, 0, inputData[1], BLOCK_SIZE);
    applySlidersControls(inputDataBuffer, inputChannel, inputPan, inputLevelDb);

    effect_slot_type slotType = getSlotType(fxdata->fxslot);
    if (slotType == a_insert_slot || slotType == b_insert_slot)
    {
            // surge->sceneout[n_scenes][n_channels][BLOCK_SIZE]
            // take a look at the data field we make for the alternate outputs
            // yeah surge->sceneout[n_scenes][n_channels][BLOCK_SIZE]
            // is the already downsampled scene output for A and B that should be populated before
            // the FX re run but if its not just move the population in SurgeSythesizer::process


        float& sceneInputChannel = fxdata->p[in_scene_input_channel].val.f;
        float& sceneInputPan = fxdata->p[in_scene_input_pan].val.f;
        float& sceneInputLevelDb = fxdata->p[in_scene_input_level].val.f;

        int otherScene = slotType == a_insert_slot ? 1 : 0;
        float* sceneData[] = { 
            storage->scenesOutputData.getSceneOut(otherScene, 0), 
            storage->scenesOutputData.getSceneOut(otherScene, 1)
        };
        juce::AudioBuffer<float> sceneDataBuffer( 2, BLOCK_SIZE);
        sceneDataBuffer.copyFrom(0, 0, sceneData[0], BLOCK_SIZE);
        sceneDataBuffer.copyFrom(1, 0, sceneData[1], BLOCK_SIZE);
        applySlidersControls(sceneDataBuffer, sceneInputChannel, sceneInputPan, sceneInputLevelDb);
        // mixing the scene audio input and the effect audio input
        effectDataBuffer.addFrom(0, 0, sceneDataBuffer, 0, 0, BLOCK_SIZE);
        effectDataBuffer.addFrom(1, 0, sceneDataBuffer, 1, 0, BLOCK_SIZE);
    }
    // mixing the effect and audio input
    effectDataBuffer.addFrom(0, 0, inputDataBuffer, 0, 0, BLOCK_SIZE);
    effectDataBuffer.addFrom(1, 0, inputDataBuffer, 1, 0, BLOCK_SIZE);
}

void AudioInputEffect::applySlidersControls(juce::AudioBuffer<float> &buffer, const float &channel,
                                            const float &pan, const float &levelDb)
{
    float leftGain, rightGain;

    if (channel < 0) {
            leftGain = 1.0f;
            rightGain = 1.0f + channel;
    } else {
            leftGain = 1.0f - channel;
            rightGain = 1.0f;
    }
    buffer.applyGain(0, 0, buffer.getNumSamples(), leftGain);
    buffer.applyGain(1, 0, buffer.getNumSamples(), rightGain);


    juce::AudioBuffer<float> tempBuffer(2, BLOCK_SIZE);
    tempBuffer.copyFrom(0, 0, buffer, 0, 0, BLOCK_SIZE);
    tempBuffer.copyFrom(1, 0, buffer, 1, 0, BLOCK_SIZE);

    float leftToLeft = (pan < 0) ? 1.0f : 1.0f - pan;
    float rightToRight = (pan > 0) ? 1.0f : 1.0f + pan;

    buffer.applyGain(0, 0, buffer.getNumSamples(), leftToLeft);
    buffer.addFrom(0, 0, tempBuffer, 1, 0, BLOCK_SIZE, 1.0f - rightToRight);

    buffer.applyGain(1, 0, buffer.getNumSamples(), rightToRight);
    buffer.addFrom(1, 0, tempBuffer, 0, 0, BLOCK_SIZE, 1.0f - leftToLeft);

    float effectInputLevelGain = juce::Decibels::decibelsToGain(levelDb);
    buffer.applyGain(effectInputLevelGain);
}