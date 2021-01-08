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

#include "Neuron.h"

namespace chowdsp
{

Neuron::Neuron(SurgeStorage* storage, FxStorage* fxdata, pdata* pd)
    : Effect(storage, fxdata, pd)
{
    dc_blocker.setBlockSize(BLOCK_SIZE);
    makeup.set_blocksize(BLOCK_SIZE);
}

Neuron::~Neuron()
{}

void Neuron::init()
{
    Wf.reset(numSteps);
    Wh.reset(numSteps);
    Uf.reset(numSteps);
    Uh.reset(numSteps);
    bf.reset(numSteps);
    delaySmooth.reset(numSteps);

    os.reset();

    delay.prepare(dsamplerate * os.getOSRatio(), BLOCK_SIZE, 2);
    delay.setDelay(0.0f);

    y1[0] = 0.0f;
    y1[1] = 0.0f;

    dc_blocker.suspend();
    dc_blocker.coeff_HP(5.0f / samplerate, 0.707);
    dc_blocker.coeff_instantize();
    makeup.set_target(1.0f);
}

void Neuron::process(float* dataL, float* dataR)
{
    set_params();

    os.upsample(dataL, dataR);
    process_internal(os.leftUp, os.rightUp, os.getUpBlockSize());
    os.downsample(dataL, dataR);

    dc_blocker.process_block(dataL, dataR);
    makeup.multiply_2_blocks(dataL, dataR, BLOCK_SIZE_QUAD);
}

void Neuron::process_internal(float* dataL, float* dataR, const int numSamples)
{
    for(int k = 0; k < numSamples; k++)
    {
        dataL[k] = processSample(dataL[k], y1[0]);
        dataR[k] = processSample(dataR[k], y1[1]);

        delay.setDelay(delaySmooth.getNextValue());
        delay.pushSample(0, dataL[k]);
        delay.pushSample(1, dataR[k]);

        y1[0] = delay.popSample(0);
        y1[1] = delay.popSample(1);
    }
}

void Neuron::suspend()
{}

void Neuron::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[neuron_wh].set_name("Drive");
    fxdata->p[neuron_wh].set_type(ct_decibel_narrow);

    fxdata->p[neuron_wf].set_name("Squash");
    fxdata->p[neuron_wf].set_type(ct_percent);

    fxdata->p[neuron_uf].set_name("Stab");
    fxdata->p[neuron_uf].set_type(ct_percent);

    fxdata->p[neuron_uh].set_name("Asymmetry");
    fxdata->p[neuron_uh].set_type(ct_percent);

    fxdata->p[neuron_bf].set_name("Bias");
    fxdata->p[neuron_bf].set_type(ct_percent);

    fxdata->p[neuron_freq].set_name("Frequency");
    fxdata->p[neuron_freq].set_type(ct_freq_audible);

    for (int i = 0; i < neuron_num_ctrls; i++)
      fxdata->p[i].posy_offset = 1;
}

void Neuron::init_default_values()
{
    fxdata->p[neuron_wf].val.f = 0.0f;
    fxdata->p[neuron_wh].val.f = 0.0f;
    fxdata->p[neuron_uf].val.f = 0.0f;
    fxdata->p[neuron_uh].val.f = 0.0f;
    fxdata->p[neuron_bf].val.f = 1.0f / 6.0f;
    fxdata->p[neuron_freq].val.f = 0.0f;
}

void Neuron::set_params()
{
    auto bf_clamped = std::clamp (*f[neuron_bf], 0.0f, 1.0f);

    Wf.setTargetValue(*f[neuron_wf] * 20.0f);
    Wh.setTargetValue(db_to_linear(*f[neuron_wh]));
    Uf.setTargetValue(*f[neuron_uf] * 5.0f);
    Uh.setTargetValue(*f[neuron_uh] * 0.9f);
    bf.setTargetValue(bf_clamped * 6.0f - 1.0f);

    // tune delay length
    auto freqHz = (2 * 3.14159265358979323846) * 440 * storage->note_to_pitch_ignoring_tuning(*f[neuron_freq]);
    auto delayTimeSec = 1.0f / (float) freqHz;
    delaySmooth.setTargetValue(delayTimeSec * samplerate * os.getOSRatio());

    // calc makeup gain
    auto drive_makeup = [] (float wh) -> float {
        return std::exp(-0.11898f * wh) + 1.0f;
    };

    auto bias_makeup = [] (float bf) -> float {
        return 6.0f * std::pow(bf, 7.5f) + 0.9f;
    };

    const auto makeupGain = drive_makeup(*f[neuron_wh]) * bias_makeup(bf_clamped);
    makeup.set_target_smoothed(makeupGain);
}

const char* Neuron::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Neuron";
    }

    return 0;
}

int Neuron::group_label_ypos(int id)
{
    switch (id)
   {
   case 0:
      return 1;
   }
   return 0;
}

} // namespace chowdsp
