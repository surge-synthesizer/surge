#include "Reverb2Effect.h"

const float db60 = powf(10.f, 0.05f * -60.f);

Reverb2Effect::allpass::allpass()
{
    _k = 0;
    _len = 1;
    memset(_data, 0, MAX_ALLPASS_LEN * sizeof(float));
}

void Reverb2Effect::allpass::setLen(int len) { _len = len; }

float Reverb2Effect::allpass::process(float in, float coeff)
{
    _k++;
    if (_k >= _len)
        _k = 0;
    float delay_in = in - coeff * _data[_k];
    float result = _data[_k] + coeff * delay_in;
    _data[_k] = delay_in;
    return result;
}

Reverb2Effect::delay::delay()
{
    _k = 0;
    _len = 1;
    memset(_data, 0, MAX_DELAY_LEN * sizeof(float));
}

void Reverb2Effect::delay::setLen(int len) { _len = len; }

float Reverb2Effect::delay::process(float in, int tap1, float &tap_out1, int tap2, float &tap_out2,
                                    int modulation)
{
    _k = (_k + 1) & DELAY_LEN_MASK;

    tap_out1 = _data[(_k - tap1) & DELAY_LEN_MASK];
    tap_out2 = _data[(_k - tap2) & DELAY_LEN_MASK];

    int modulation_int = modulation >> DELAY_SUBSAMPLE_BITS;
    int modulation_frac1 = modulation & (DELAY_SUBSAMPLE_RANGE - 1);
    int modulation_frac2 = DELAY_SUBSAMPLE_RANGE - modulation_frac1;

    float d1 = _data[(_k - _len + modulation_int + 1) & DELAY_LEN_MASK];
    float d2 = _data[(_k - _len + modulation_int) & DELAY_LEN_MASK];
    const float multiplier = 1.f / (float)(DELAY_SUBSAMPLE_RANGE);

    float result = (d1 * (float)modulation_frac1 + d2 * (float)modulation_frac2) * multiplier;
    _data[_k] = in;

    return result;
}

Reverb2Effect::onepole_filter::onepole_filter() { a0 = 0.f; }

float Reverb2Effect::onepole_filter::process_lowpass(float x, float c0)
{
    a0 = a0 * c0 + x * (1.f - c0);
    return a0;
}

float Reverb2Effect::onepole_filter::process_highpass(float x, float c0)
{
    a0 = a0 * (1.f - c0) + x * c0;
    return x - a0;
}

Reverb2Effect::Reverb2Effect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd)
{
    _state = 0.f;
}

Reverb2Effect::~Reverb2Effect() {}

void Reverb2Effect::init() { setvars(true); }

int msToSamples(float ms, float scale, float samplerate)
{
    float a = samplerate * ms * 0.001f;
    float b = a * scale;
    return (int)(b);
}

void Reverb2Effect::calc_size(float scale)
{
    float m = scale;

    _tap_timeL[0] = msToSamples(80.3, m, storage->samplerate);
    _tap_timeL[1] = msToSamples(59.3, m, storage->samplerate);
    _tap_timeL[2] = msToSamples(97.7, m, storage->samplerate);
    _tap_timeL[3] = msToSamples(122.6, m, storage->samplerate);
    _tap_timeR[0] = msToSamples(35.5, m, storage->samplerate);
    _tap_timeR[1] = msToSamples(101.6, m, storage->samplerate);
    _tap_timeR[2] = msToSamples(73.9, m, storage->samplerate);
    _tap_timeR[3] = msToSamples(80.3, m, storage->samplerate);

    _input_allpass[0].setLen(msToSamples(4.76, m, storage->samplerate));
    _input_allpass[1].setLen(msToSamples(6.81, m, storage->samplerate));
    _input_allpass[2].setLen(msToSamples(10.13, m, storage->samplerate));
    _input_allpass[3].setLen(msToSamples(16.72, m, storage->samplerate));

    _allpass[0][0].setLen(msToSamples(38.2, m, storage->samplerate));
    _allpass[0][1].setLen(msToSamples(53.4, m, storage->samplerate));
    _delay[0].setLen(msToSamples(178.8, m, storage->samplerate));

    _allpass[1][0].setLen(msToSamples(44.0, m, storage->samplerate));
    _allpass[1][1].setLen(msToSamples(41, m, storage->samplerate));
    _delay[1].setLen(msToSamples(126.5, m, storage->samplerate));

    _allpass[2][0].setLen(msToSamples(48.3, m, storage->samplerate));
    _allpass[2][1].setLen(msToSamples(60.5, m, storage->samplerate));
    _delay[2].setLen(msToSamples(106.1, m, storage->samplerate));

    _allpass[3][0].setLen(msToSamples(38.9, m, storage->samplerate));
    _allpass[3][1].setLen(msToSamples(42.2, m, storage->samplerate));
    _delay[3].setLen(msToSamples(139.4, m, storage->samplerate));
}

void Reverb2Effect::setvars(bool init)
{
    // TODO: balance the gains from the calculated decay coefficient?
    _tap_gainL[0] = 1.5f / 4.f;
    _tap_gainL[1] = 1.2f / 4.f;
    _tap_gainL[2] = 1.0f / 4.f;
    _tap_gainL[3] = 0.8f / 4.f;
    _tap_gainR[0] = 1.5f / 4.f;
    _tap_gainR[1] = 1.2f / 4.f;
    _tap_gainR[2] = 1.0f / 4.f;
    _tap_gainR[3] = 0.8f / 4.f;

    calc_size(1.f);
}

void Reverb2Effect::update_rtime()
{
    auto ts = fxdata->p[rev2_predelay].temposync ? storage->temposyncratio_inv : 1.f;
    // * 2.f is to get the dB120 time
    auto pdlyt = std::max(0.1f, powf(2.f, *f[rev2_predelay]) * ts) * 2.f;
    auto dcyt = std::max(1.0f, powf(2.f, *f[rev2_decay_time])) * 2.f;
    float t = BLOCK_SIZE_INV * (storage->samplerate * (dcyt + pdlyt));

    ringout_time = (int)t;
}

void Reverb2Effect::process(float *dataL, float *dataR)
{
    float scale = powf(2.f, 1.f * *f[rev2_room_size]);
    calc_size(scale);

    if (fabs(*f[rev2_decay_time] - last_decay_time) > 0.001f)
        update_rtime();

    last_decay_time = *f[rev2_decay_time];

    float wetL alignas(16)[BLOCK_SIZE], wetR alignas(16)[BLOCK_SIZE];

    float loop_time_s = 0.5508 * scale;
    float decay = powf(db60, loop_time_s / (4.f * (powf(2.f, *f[rev2_decay_time]))));

    _decay_multiply.newValue(decay);
    _diffusion.newValue(0.7f * *f[rev2_diffusion]);
    _buildup.newValue(0.7f * *f[rev2_buildup]);
    _hf_damp_coefficent.newValue(0.8 * *f[rev2_hf_damping]);
    _lf_damp_coefficent.newValue(0.2 * *f[rev2_lf_damping]);
    _modulation.newValue(*f[rev2_modulation] * storage->samplerate * 0.001f * 5.f);

    width.set_target_smoothed(storage->db_to_linear(*f[rev2_width]));
    mix.set_target_smoothed(*f[rev2_mix]);

    _lfo.set_rate(2.0 * M_PI * powf(2, -2.f) * storage->dsamplerate_inv);

    int pdt =
        limit_range((int)(storage->samplerate * pow(2.f, *f[rev2_predelay]) *
                          (fxdata->p[rev2_predelay].temposync ? storage->temposyncratio_inv : 1.f)),
                    1, PREDELAY_BUFFER_SIZE_LIMIT - 1);

    for (int k = 0; k < BLOCK_SIZE; k++)
    {
        float in = (dataL[k] + dataR[k]) * 0.5f;

        in = _predelay.process(in, pdt);

        in = _input_allpass[0].process(in, _diffusion.v);
        in = _input_allpass[1].process(in, _diffusion.v);
        in = _input_allpass[2].process(in, _diffusion.v);
        in = _input_allpass[3].process(in, _diffusion.v);
        float x = _state;

        float outL = 0.f;
        float outR = 0.f;

        float lfos[NUM_BLOCKS];
        lfos[0] = _lfo.r;
        lfos[1] = _lfo.i;
        lfos[2] = -_lfo.r;
        lfos[3] = -_lfo.i;

        auto hdc = limit_range(_hf_damp_coefficent.v, 0.01f, 0.99f);
        auto ldc = limit_range(_lf_damp_coefficent.v, 0.01f, 0.99f);
        for (int b = 0; b < NUM_BLOCKS; b++)
        {
            x = x + in;
            for (int c = 0; c < NUM_ALLPASSES_PER_BLOCK; c++)
            {
                x = _allpass[b][c].process(x, _buildup.v);
            }

            x = _hf_damper[b].process_lowpass(x, hdc);
            x = _lf_damper[b].process_highpass(x, ldc);

            int modulation = (int)(_modulation.v * lfos[b] * (float)DELAY_SUBSAMPLE_RANGE);
            float tap_outL = 0.f;
            float tap_outR = 0.f;
            x = _delay[b].process(x, _tap_timeL[b], tap_outL, _tap_timeR[b], tap_outR, modulation);
            outL += tap_outL * _tap_gainL[b];
            outR += tap_outR * _tap_gainR[b];

            x *= _decay_multiply.v;
        }

        wetL[k] = outL;
        wetR[k] = outR;
        _state = x;
        _decay_multiply.process();
        _diffusion.process();
        _buildup.process();
        _hf_damp_coefficent.process();
        _lfo.process();
        _modulation.process();
    }

    // scale width
    float M alignas(16)[BLOCK_SIZE], S alignas(16)[BLOCK_SIZE];
    encodeMS(wetL, wetR, M, S, BLOCK_SIZE_QUAD);
    width.multiply_block(S, BLOCK_SIZE_QUAD);
    decodeMS(M, S, wetL, wetR, BLOCK_SIZE_QUAD);

    mix.fade_2_blocks_to(dataL, wetL, dataR, wetR, dataL, dataR, BLOCK_SIZE_QUAD);
}

void Reverb2Effect::suspend() { init(); }

const char *Reverb2Effect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Pre-Delay";
    case 1:
        return "Reverb";
    case 2:
        return "EQ";
    case 3:
        return "Output";
    }
    return 0;
}
int Reverb2Effect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 5;
    case 2:
        return 17;
    case 3:
        return 23;
    }
    return 0;
}

void Reverb2Effect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[rev2_predelay].set_name("Pre-Delay");
    fxdata->p[rev2_predelay].set_type(ct_reverbpredelaytime);

    fxdata->p[rev2_room_size].set_name("Room Size");
    fxdata->p[rev2_room_size].set_type(ct_percent_bipolar);
    fxdata->p[rev2_decay_time].set_name("Decay Time");
    fxdata->p[rev2_decay_time].set_type(ct_reverbtime);
    fxdata->p[rev2_diffusion].set_name("Diffusion");
    fxdata->p[rev2_diffusion].set_type(ct_percent);
    fxdata->p[rev2_buildup].set_name("Buildup");
    fxdata->p[rev2_buildup].set_type(ct_percent);
    fxdata->p[rev2_modulation].set_name("Modulation");
    fxdata->p[rev2_modulation].set_type(ct_percent);

    fxdata->p[rev2_hf_damping].set_name("HF Damping");
    fxdata->p[rev2_hf_damping].set_type(ct_percent);
    fxdata->p[rev2_lf_damping].set_name("LF Damping");
    fxdata->p[rev2_lf_damping].set_type(ct_percent);

    fxdata->p[rev2_width].set_name("Width");
    fxdata->p[rev2_width].set_type(ct_decibel_narrow);
    fxdata->p[rev2_mix].set_name("Mix");
    fxdata->p[rev2_mix].set_type(ct_percent);

    for (int i = rev2_predelay; i < rev2_num_params; ++i)
    {
        auto a = 1;
        if (i >= rev2_room_size)
            a += 2;
        if (i >= rev2_lf_damping)
            a += 2;
        if (i >= rev2_width)
            a += 2;
        fxdata->p[i].posy_offset = a;
    }
}

void Reverb2Effect::init_default_values()
{
    fxdata->p[rev2_predelay].val.f = -4.f;
    fxdata->p[rev2_decay_time].val.f = 0.75f;
    fxdata->p[rev2_mix].val.f = 0.33f;
    fxdata->p[rev2_width].val.f = 0.0f;
    fxdata->p[rev2_diffusion].val.f = 1.0f;
    fxdata->p[rev2_buildup].val.f = 1.0f;
    fxdata->p[rev2_modulation].val.f = 0.5f;
    fxdata->p[rev2_hf_damping].val.f = 0.2f;
    fxdata->p[rev2_lf_damping].val.f = 0.2f;
    fxdata->p[rev2_room_size].val.f = 0.f;
}
