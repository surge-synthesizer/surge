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

#include "BBDEnsembleEffect.h"

std::string ensemble_stage_name(int i)
{
    switch (i)
    {
    case BBDEnsembleEffect::ens_sinc:
        return "Digital Delay";
    case BBDEnsembleEffect::ens_128:
        return "BBD 128 Stages";
    case BBDEnsembleEffect::ens_256:
        return "BBD 256 Stages";
    case BBDEnsembleEffect::ens_512:
        return "BBD 512 Stages";
    case BBDEnsembleEffect::ens_1024:
        return "BBD 1024 Stages";
    case BBDEnsembleEffect::ens_2048:
        return "BBD 2048 Stages";
    case BBDEnsembleEffect::ens_4096:
        return "BBD 4096 Stages";
    }
    return "Error";
}

int ensemble_num_stages(int i)
{
    switch (i)
    {
    case BBDEnsembleEffect::ens_sinc:
        return 512; // faking it :)!
    case BBDEnsembleEffect::ens_128:
        return 128;
    case BBDEnsembleEffect::ens_256:
        return 256;
    case BBDEnsembleEffect::ens_512:
        return 512;
    case BBDEnsembleEffect::ens_1024:
        return 1024;
    case BBDEnsembleEffect::ens_2048:
        return 2048;
    case BBDEnsembleEffect::ens_4096:
        return 4096;
    }
    return 1;
}

int ensemble_stage_count() { return 7; }

float calculateFilterParamFrequency(float *f, SurgeStorage *storage)
{
    auto param_val = f[BBDEnsembleEffect::ens_input_filter];
    auto clock_rate = f[BBDEnsembleEffect::ens_delay_clockrate];
    auto freq = 2.0f * (float)M_PI * 400.0f * storage->note_to_pitch_ignoring_tuning(param_val);
    auto freq_adjust = freq * std::pow(clock_rate * 0.01f, 0.75f);
    return std::min(freq_adjust, 25000.0f);
}

namespace
{
constexpr float delay1Ms = 0.6f;
constexpr float delay2Ms = 0.2f;

constexpr float qVals2ndOrderButter[] = {1.0 / 0.7654, 1.0 / 1.8478};
} // namespace

BBDEnsembleEffect::BBDEnsembleEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd), delL(storage->sinctable), delR(storage->sinctable)
{
    width.set_blocksize(BLOCK_SIZE);
    mix.set_blocksize(BLOCK_SIZE);

    for (int i = 0; i < 2; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            modlfos[i][j].samplerate = storage->samplerate;
            modlfos[i][j].samplerate_inv = storage->samplerate_inv;
        }
    }

    for (int i = 0; i < 2; ++i)
        dc_blocker[i].setBlockSize(BLOCK_SIZE);
}

BBDEnsembleEffect::~BBDEnsembleEffect() {}

void BBDEnsembleEffect::init()
{
    setvars(true);
    block_counter = 0;

    auto init_bbds = [=](auto &delL1, auto &delL2, auto &delR1, auto &delR2) {
        for (auto *del : {&delL1, &delL2, &delR1, &delR2})
        {
            del->prepare(storage->samplerate);
            del->setFilterFreq(10000.0f);
            del->setDelayTime(0.005f);
        }
    };

    init_bbds(del_128L1, del_128L2, del_128R1, del_128R2);
    init_bbds(del_256L1, del_256L2, del_256R1, del_256R2);
    init_bbds(del_512L1, del_512L2, del_512R1, del_512R2);
    init_bbds(del_1024L1, del_1024L2, del_1024R1, del_1024R2);
    init_bbds(del_2048L1, del_2048L2, del_2048R1, del_2048R2);
    init_bbds(del_4096L1, del_4096L2, del_4096R1, del_4096R2);

    bbd_saturation_sse.reset(storage->samplerate);
    bbd_saturation_sse.setDrive(0.5f);

    fbStateL = 0.0f;
    fbStateR = 0.0f;

    // Butterworth highpass
    const auto dc_omega = 2 * M_PI * 50.0 / storage->samplerate;
    for (int i = 0; i < 2; ++i)
    {
        dc_blocker[i].suspend();
        dc_blocker[i].coeff_HP(dc_omega, qVals2ndOrderButter[i]);
        dc_blocker[i].coeff_instantize();
    }

    const auto hf_omega = 2 * M_PI * 20000.0 / storage->samplerate;
    sincInputFilter.suspend();
    sincInputFilter.coeff_LP(hf_omega, 0.7071);
    sincInputFilter.coeff_instantize();
}

void BBDEnsembleEffect::setvars(bool init)
{
    if (init)
    {
        width.set_target(0.f);
        mix.set_target(1.f);

        width.instantize();
        mix.instantize();
    }
}

float BBDEnsembleEffect::getFeedbackGain(bool bbd) const noexcept
{
    // normally we would just need the feedback to be less than 1
    // however here we're adding the output of 2 delay lines so
    // we need the feedback to be less than 0.5.
    auto baseFeedbackParam = (bbd ? 0.2f : 1.0f) * *f[ens_delay_feedback];
    return 0.49f * std::pow(baseFeedbackParam, 0.5f);
}

void BBDEnsembleEffect::process_sinc_delays(float *dataL, float *dataR, float delayCenterMs,
                                            float delayScale)
{
    // copy input data ("dry") to processed output ("wet)
    copy_block(dataL, L, BLOCK_SIZE_QUAD);
    copy_block(dataR, R, BLOCK_SIZE_QUAD);

    const auto aa_cutoff = calculateFilterParamFrequency(*f, storage);
    sincInputFilter.coeff_LP(2 * M_PI * aa_cutoff / storage->samplerate, 0.7071);
    sincInputFilter.process_block(L, R);

    float del1 = delayScale * delay1Ms * 0.001 * storage->samplerate;
    float del2 = delayScale * delay2Ms * 0.001 * storage->samplerate;
    float del0 = delayCenterMs * 0.001 * storage->samplerate;

    bbd_saturation_sse.setDrive(*f[ens_delay_sat]);
    float fbGain = getFeedbackGain(false);

    for (int s = 0; s < BLOCK_SIZE; ++s)
    {
        // reduce input by 3 dB
        L[s] *= 0.75f;
        R[s] *= 0.75f;

        // soft-clip input
        L[s] =
            storage->lookup_waveshape(sst::waveshapers::WaveshaperType::wst_soft, L[s] + fbStateL);
        R[s] =
            storage->lookup_waveshape(sst::waveshapers::WaveshaperType::wst_soft, R[s] + fbStateR);

        delL.write(L[s]);
        delR.write(R[s]);

        // OK so look at the diagram in #3743
        float t1 = del1 * modlfos[0][0].value() + del2 * modlfos[1][0].value() + del0;
        float t2 = del1 * modlfos[0][1].value() + del2 * modlfos[1][1].value() + del0;
        float t3 = del1 * modlfos[0][2].value() + del2 * modlfos[1][2].value() + del0;

        float ltap1 = t1;
        float ltap2 = t2;
        float rtap1 = t2;
        float rtap2 = t3;

        float delayOuts alignas(16)[4];
        delayOuts[0] = delL.read(ltap1);
        delayOuts[1] = delL.read(ltap2);
        delayOuts[2] = delR.read(rtap1);
        delayOuts[3] = delR.read(rtap2);

        fbStateL = fbGain * (delayOuts[0] + delayOuts[1]);
        fbStateR = fbGain * (delayOuts[1] + delayOuts[2]);
        // avoid DC build-up in the feedback path
        dc_blocker[0].process_sample_nolag(fbStateL, fbStateR);
        dc_blocker[1].process_sample_nolag(fbStateL, fbStateR);

        auto delayOutsVec = vLoad(delayOuts);
        auto waveshaperOutsVec = bbd_saturation_sse.processSample(delayOutsVec);

        float waveshaperOuts alignas(16)[4];
        _mm_store_ps(waveshaperOuts, waveshaperOutsVec);
        L[s] = waveshaperOuts[0] + waveshaperOuts[1];
        R[s] = waveshaperOuts[2] + waveshaperOuts[3];

        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 2; ++j)
                modlfos[j][i].post_process();
    }
}

void BBDEnsembleEffect::process(float *dataL, float *dataR)
{
    setvars(false);

    // limit between 0.005 and 40 Hz with modulation
    double rate1 =
        storage->envelope_rate_linear(-limit_range(*f[ens_lfo_freq1], -7.64386f, 5.322f));
    double rate2 =
        storage->envelope_rate_linear(-limit_range(*f[ens_lfo_freq2], -7.64386f, 5.322f));
    float roff = 0;
    constexpr float onethird = 1.0 / 3.0;

    for (int i = 0; i < 3; ++i)
    {
        modlfos[0][i].pre_process(Surge::ModControl::mod_sine, rate1, *f[ens_lfo_depth1], roff);
        modlfos[1][i].pre_process(Surge::ModControl::mod_sine, rate2, *f[ens_lfo_depth2], roff);

        roff += onethird;
    }

    auto bbd_stages = *pdata_ival[ens_delay_type];
    const auto clock_rate = std::max(*f[ens_delay_clockrate], 1.0f); // make sure this is not zero!
    const auto numStages = ensemble_num_stages(bbd_stages);
    const auto delayCenterMs = (float)numStages / (2.0f * clock_rate);
    const auto delayScale =
        0.95f * delayCenterMs / (delay1Ms + delay2Ms); // make sure total delay is always positive

    auto process_bbd_delays = [=](float *dataL, float *dataR, auto &delL1, auto &delL2, auto &delR1,
                                  auto &delR2) {
        // copy input data ("dry") to processed output ("wet)
        copy_block(dataL, L, BLOCK_SIZE_QUAD);
        copy_block(dataR, R, BLOCK_SIZE_QUAD);

        // setting the filter frequency takes a while, so
        // let's only do it every 4 times
        if (block_counter++ == 3)
        {
            const auto aa_cutoff = calculateFilterParamFrequency(*f, storage);
            for (auto *del : {&delL1, &delL2, &delR1, &delR2})
                del->setFilterFreq(aa_cutoff);

            block_counter = 0;
        }

        bbd_saturation_sse.setDrive(*f[ens_delay_sat]);
        float fbGain = getFeedbackGain(true);

        float del1 = delayScale * delay1Ms * 0.001;
        float del2 = delayScale * delay2Ms * 0.001;
        float del0 = delayCenterMs * 0.001;

        for (int s = 0; s < BLOCK_SIZE; ++s)
        {
            // reduce input by 3 dB
            L[s] *= 0.75f;
            R[s] *= 0.75f;

            // soft-clip input
            L[s] = storage->lookup_waveshape(sst::waveshapers::WaveshaperType::wst_soft,
                                             L[s] + fbStateL);
            R[s] = storage->lookup_waveshape(sst::waveshapers::WaveshaperType::wst_soft,
                                             R[s] + fbStateR);

            // OK so look at the diagram in #3743
            float t1 = del1 * modlfos[0][0].value() + del2 * modlfos[1][0].value() + del0;
            float t2 = del1 * modlfos[0][1].value() + del2 * modlfos[1][1].value() + del0;
            float t3 = del1 * modlfos[0][2].value() + del2 * modlfos[1][2].value() + del0;

            delL1.setDelayTime(t1);
            delL2.setDelayTime(t2);
            delR1.setDelayTime(t2);
            delR2.setDelayTime(t3);

            float delayOuts alignas(16)[4];
            delayOuts[0] = delL1.process(L[s]);
            delayOuts[1] = delL2.process(L[s]);
            delayOuts[2] = delR1.process(R[s]);
            delayOuts[3] = delR2.process(R[s]);

            fbStateL = fbGain * (delayOuts[0] + delayOuts[1]);
            fbStateR = fbGain * (delayOuts[1] + delayOuts[2]);

            // avoid DC build-up in the feedback path
            dc_blocker[0].process_sample_nolag(fbStateL, fbStateR);
            dc_blocker[1].process_sample_nolag(fbStateL, fbStateR);

            auto delayOutsVec = vLoad(delayOuts);
            auto waveshaperOutsVec = bbd_saturation_sse.processSample(delayOutsVec);

            float waveshaperOuts alignas(16)[4];
            _mm_store_ps(waveshaperOuts, waveshaperOutsVec);
            L[s] = waveshaperOuts[0] + waveshaperOuts[1];
            R[s] = waveshaperOuts[2] + waveshaperOuts[3];

            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 2; ++j)
                    modlfos[j][i].post_process();
        }

        mul_block(L, storage->db_to_linear(-8.0f), L, BLOCK_SIZE_QUAD);
        mul_block(R, storage->db_to_linear(-8.0f), R, BLOCK_SIZE_QUAD);
    };

    switch (bbd_stages)
    {
    case ens_sinc:
        process_sinc_delays(dataL, dataR, delayCenterMs, delayScale);
        break;
    case ens_128:
        process_bbd_delays(dataL, dataR, del_128L1, del_128L2, del_128R1, del_128R2);
        break;
    case ens_256:
        process_bbd_delays(dataL, dataR, del_256L1, del_256L2, del_256R1, del_256R2);
        break;
    case ens_512:
        process_bbd_delays(dataL, dataR, del_512L1, del_512L2, del_512R1, del_512R2);
        break;
    case ens_1024:
        process_bbd_delays(dataL, dataR, del_1024L1, del_1024L2, del_1024R1, del_1024R2);
        break;
    case ens_2048:
        process_bbd_delays(dataL, dataR, del_2048L1, del_2048L2, del_2048R1, del_2048R2);
        break;
    case ens_4096:
        process_bbd_delays(dataL, dataR, del_4096L1, del_4096L2, del_4096R1, del_4096R2);
        break;
    }

    // scale width
    width.set_target_smoothed(clamp1bp(*f[ens_width]));

    float M alignas(16)[BLOCK_SIZE], S alignas(16)[BLOCK_SIZE];
    encodeMS(L, R, M, S, BLOCK_SIZE_QUAD);
    width.multiply_block(S, BLOCK_SIZE_QUAD);
    decodeMS(M, S, L, R, BLOCK_SIZE_QUAD);

    mix.set_target_smoothed(clamp1bp(*f[ens_mix]));
    mix.fade_2_blocks_to(dataL, L, dataR, R, dataL, dataR, BLOCK_SIZE_QUAD);
}

void BBDEnsembleEffect::suspend() { init(); }

const char *BBDEnsembleEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Input";
    case 1:
        return "Modulation";
    case 2:
        return "Delay";
    case 3:
        return "Output";
    }
    return 0;
}

int BBDEnsembleEffect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 5;
    case 2:
        return 15;
    case 3:
        return 25;
    }
    return 0;
}

void BBDEnsembleEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[ens_input_filter].set_name("Filter");
    fxdata->p[ens_input_filter].set_type(ct_freq_audible);
    fxdata->p[ens_input_filter].val_default.f = 3.65f * 12.f;
    fxdata->p[ens_input_filter].posy_offset = 1;

    fxdata->p[ens_lfo_freq1].set_name("Frequency 1");
    fxdata->p[ens_lfo_freq1].set_type(ct_ensemble_lforate);
    fxdata->p[ens_lfo_freq1].posy_offset = 3;
    fxdata->p[ens_lfo_depth1].set_name("Depth 1");
    fxdata->p[ens_lfo_depth1].set_type(ct_percent);
    fxdata->p[ens_lfo_depth1].posy_offset = 3;
    fxdata->p[ens_lfo_freq2].set_name("Frequency 2");
    fxdata->p[ens_lfo_freq2].set_type(ct_ensemble_lforate);
    fxdata->p[ens_lfo_freq2].posy_offset = 3;
    fxdata->p[ens_lfo_depth2].set_name("Depth 2");
    fxdata->p[ens_lfo_depth2].set_type(ct_percent);
    fxdata->p[ens_lfo_depth2].posy_offset = 3;

    fxdata->p[ens_delay_type].set_name("Type");
    fxdata->p[ens_delay_type].set_type(ct_ensemble_stages);
    fxdata->p[ens_delay_type].posy_offset = 5;
    fxdata->p[ens_delay_clockrate].set_name("Clock Rate");
    fxdata->p[ens_delay_clockrate].set_type(ct_ensemble_clockrate);
    fxdata->p[ens_delay_clockrate].posy_offset = 5;
    fxdata->p[ens_delay_sat].set_name("Saturation");
    fxdata->p[ens_delay_sat].set_type(ct_percent);
    fxdata->p[ens_delay_sat].val_default.f = 0.0f;
    fxdata->p[ens_delay_sat].posy_offset = 5;
    fxdata->p[ens_delay_feedback].set_name("Feedback");
    fxdata->p[ens_delay_feedback].set_type(ct_percent);
    fxdata->p[ens_delay_feedback].val_default.f = 0.f;
    fxdata->p[ens_delay_feedback].posy_offset = 5;

    fxdata->p[ens_width].set_name("Width");
    fxdata->p[ens_width].set_type(ct_percent_bipolar);
    fxdata->p[ens_width].val_default.f = 1.f;
    fxdata->p[ens_width].posy_offset = 7;
    fxdata->p[ens_mix].set_name("Mix");
    fxdata->p[ens_mix].set_type(ct_percent);
    fxdata->p[ens_mix].val_default.f = 1.f;
    fxdata->p[ens_mix].posy_offset = 7;
}

void BBDEnsembleEffect::init_default_values()
{
    fxdata->p[ens_input_filter].val.f = 3.65f * 12.f;

    // we want LFO 1 at .18 Hz and LFO 2 at 5.52 Hz
    // these go as 2^x so...
    fxdata->p[ens_lfo_freq1].val.f = log2(0.18);
    fxdata->p[ens_lfo_freq2].val.f = log2(5.52);

    fxdata->p[ens_lfo_depth1].val.f = 1.f;
    fxdata->p[ens_lfo_depth2].val.f = 0.6f;

    fxdata->p[ens_delay_type].val.i = 2;
    fxdata->p[ens_delay_clockrate].val.f = 40.f;
    fxdata->p[ens_delay_sat].val.f = 0.0f;
    fxdata->p[ens_delay_feedback].val.f = 0.f;

    fxdata->p[ens_width].val.f = 1.f;
    fxdata->p[ens_mix].val.f = 1.f;
}
