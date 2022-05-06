#include "PhaserEffect.h"

using namespace vt_dsp;

float bend(float x, float b) { return (1.f + b) * x - b * x * x * x; }

PhaserEffect::PhaserEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd), lp(storage), hp(storage),
      modLFOL(storage->samplerate, storage->samplerate_inv),
      modLFOR(storage->samplerate, storage->samplerate_inv)
{
    for (int i = 0; i < n_bq_units; i++)
    {
        biquad[i] = new BiquadFilter(storage);
    }

    n_bq_units_initialised = n_bq_units;
    feedback.setBlockSize(BLOCK_SIZE * slowrate);
    tone.setBlockSize(BLOCK_SIZE);
    width.set_blocksize(BLOCK_SIZE);
    lp.setBlockSize(BLOCK_SIZE);
    hp.setBlockSize(BLOCK_SIZE);
    mix.set_blocksize(BLOCK_SIZE);
    bi = 0;
}

PhaserEffect::~PhaserEffect()
{
    for (int i = 0; i < n_bq_units_initialised; i++)
        delete biquad[i];
}

void PhaserEffect::init()
{
    bi = 0;
    dL = 0;
    dR = 0;

    for (int i = 0; i < n_bq_units_initialised; i++)
    {
        biquad[i]->suspend();
    }

    clear_block(L, BLOCK_SIZE_QUAD);
    clear_block(R, BLOCK_SIZE_QUAD);

    lp.suspend();
    hp.suspend();

    lp.coeff_instantize();
    hp.coeff_instantize();

    mix.set_target(1.f);

    tone.instantize();
    width.instantize();
    mix.instantize();
}

inline void PhaserEffect::init_stages()
{
    n_stages = fxdata->p[ph_stages].val.i;
    n_bq_units = n_stages * 2;

    if (n_bq_units_initialised < n_bq_units)
    {
        // we need to increase the number of stages
        for (int k = n_bq_units_initialised; k < n_bq_units; k++)
        {
            biquad[k] = new BiquadFilter(storage);
        }

        n_bq_units_initialised = n_bq_units;
    }
}

void PhaserEffect::process_only_control()
{
    init_stages();

    modLFOL.post_process();
    modLFOR.post_process();
}

void PhaserEffect::setvars()
{
    init_stages();

    double rate = storage->envelope_rate_linear(-*f[ph_mod_rate]) *
                  (fxdata->p[ph_mod_rate].temposync ? storage->temposyncratio : 1.f);

    rate *= (float)slowrate;

    int mwave = *pdata_ival[ph_mod_wave];
    float depth = limit_range(*f[ph_mod_depth], 0.f, 2.f);

    if (fxdata->p[ph_mod_rate].deactivated)
    {
        auto rmin = fxdata->p[ph_mod_rate].val_min.f;
        auto rmax = fxdata->p[ph_mod_rate].val_max.f;
        auto phase = clamp01((*f[ph_mod_rate] - rmin) / (rmax - rmin));

        modLFOL.pre_process(mwave, 0.f, depth, phase);
        modLFOR.pre_process(mwave, 0.f, depth, phase + 0.5 * *f[ph_stereo]);
    }
    else
    {
        modLFOL.pre_process(mwave, rate, depth, 0.f);
        modLFOR.pre_process(mwave, rate, depth, 0.5 * *f[ph_stereo]);
    }

    // if stages is set to 1 to indicate we are in legacy mode, use legacy freqs and spans
    if (n_stages < 2)
    {
        // 4 stages in original phaser mode
        for (int i = 0; i < 2; i++)
        {
            double omega = biquad[2 * i]->calc_omega(2 * *f[ph_center] + legacy_freq[i] +
                                                     legacy_span[i] * modLFOL.value());
            biquad[2 * i]->coeff_APF(omega, 1.0 + 0.8 * *f[ph_sharpness]);
            omega = biquad[2 * i + 1]->calc_omega(2 * *f[ph_center] + legacy_freq[i] +
                                                  legacy_span[i] * modLFOR.value());
            biquad[2 * i + 1]->coeff_APF(omega, 1.0 + 0.8 * *f[ph_sharpness]);
        }
    }
    else
    {
        for (int i = 0; i < n_stages; i++)
        {
            double center = powf(2, (i + 1.0) * 2 / n_stages);
            double omega = biquad[2 * i]->calc_omega(2 * *f[ph_center] + *f[ph_spread] * center +
                                                     2.0 / (i + 1) * modLFOL.value());
            biquad[2 * i]->coeff_APF(omega, 1.0 + 0.8 * *f[ph_sharpness]);
            omega = biquad[2 * i + 1]->calc_omega(2 * *f[ph_center] + *f[ph_spread] * center +
                                                  (2.0 / (i + 1) * modLFOR.value()));
            biquad[2 * i + 1]->coeff_APF(omega, 1.0 + 0.8 * *f[ph_sharpness]);
        }
    }

    feedback.newValue(0.95f * *f[ph_feedback]);
    tone.newValue(clamp1bp(*f[ph_tone]));
    width.set_target_smoothed(storage->db_to_linear(*f[ph_width]));

    // lowpass range is from MIDI note 136 down to 57 (~21.1 kHz down to 220 Hz)
    // highpass range is from MIDI note 34 to 136(~61 Hz to ~21.1 kHz)
    float clo = -12, cmid = 67, chi = -33;
    float hpCutoff = chi;
    float lpCutoff = cmid;

    if (tone.v > 0)
    {
        // OK so cool scale the hp cutoff
        auto tv = tone.v;
        hpCutoff = tv * (cmid - chi) + chi;
    }
    else
    {
        auto tv = -tone.v;
        lpCutoff = tv * (clo - cmid) + cmid;
    }

    lp.coeff_LP(lp.calc_omega((lpCutoff / 12.0) - 2.f), 0.707);
    hp.coeff_HP(hp.calc_omega((hpCutoff / 12.0) - 2.f), 0.707);
}

void PhaserEffect::process(float *dataL, float *dataR)
{
    if (bi == 0)
    {
        setvars();
    }

    bi = (bi + 1) & slowrate_m1;

    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        feedback.process();
        tone.process();

        dL = dataL[i] + dL * feedback.v;
        dR = dataR[i] + dR * feedback.v;
        dL = limit_range(dL, -32.f, 32.f);
        dR = limit_range(dR, -32.f, 32.f);

        for (int curr_stage = 0; curr_stage < n_stages; curr_stage++)
        {
            dL = biquad[2 * curr_stage]->process_sample(dL);
            dR = biquad[2 * curr_stage + 1]->process_sample(dR);
        }

        L[i] = dL;
        R[i] = dR;
    }

    if (!fxdata->p[ph_tone].deactivated)
    {
        lp.process_block(L, R);
        hp.process_block(L, R);
    }

    // scale width
    float M alignas(16)[BLOCK_SIZE], S alignas(16)[BLOCK_SIZE];
    encodeMS(L, R, M, S, BLOCK_SIZE_QUAD);
    width.multiply_block(S, BLOCK_SIZE_QUAD);
    decodeMS(M, S, L, R, BLOCK_SIZE_QUAD);

    mix.set_target_smoothed(clamp01(*f[ph_mix]));
    mix.fade_2_blocks_to(dataL, L, dataR, R, dataL, dataR, BLOCK_SIZE_QUAD);
}

void PhaserEffect::suspend() { init(); }

const char *PhaserEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Modulation";
    case 1:
        return "Stages";
    case 2:
        return "Filter";
    case 3:
        return "Output";
    }
    return 0;
}
int PhaserEffect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 11;
    case 2:
        return 23;
    case 3:
        return 27;
    }
    return 0;
}

void PhaserEffect::init_ctrltypes()
{
    static struct PhaserDeactivate : public ParameterDynamicDeactivationFunction
    {
        const bool getValue(const Parameter *p) const override
        {
            auto fx = &(p->storage->getPatch().fx[p->ctrlgroup_entry]);
            auto idx = p - fx->p;

            if (idx == ph_spread)
            {
                return fx->p[ph_stages].val.i == 1;
            }

            return false;
        }
    } phGroupDeact;

    Effect::init_ctrltypes();

    fxdata->p[ph_mod_wave].set_name("Waveform");
    fxdata->p[ph_mod_wave].set_type(ct_fxlfowave);
    fxdata->p[ph_mod_rate].set_name("Rate");
    fxdata->p[ph_mod_rate].set_type(ct_lforate_deactivatable);
    fxdata->p[ph_mod_depth].set_name("Depth");
    fxdata->p[ph_mod_depth].set_type(ct_percent);
    fxdata->p[ph_stereo].set_name("Stereo");
    fxdata->p[ph_stereo].set_type(ct_percent);

    fxdata->p[ph_stages].set_name("Count");
    fxdata->p[ph_stages].set_type(ct_phaser_stages);
    fxdata->p[ph_spread].set_name("Spread");
    fxdata->p[ph_spread].set_type(ct_percent);
    fxdata->p[ph_center].set_name("Center");
    fxdata->p[ph_center].set_type(ct_percent_bipolar);
    fxdata->p[ph_sharpness].set_name("Sharpness");
    fxdata->p[ph_sharpness].set_type(ct_percent_bipolar);
    fxdata->p[ph_feedback].set_name("Feedback");
    fxdata->p[ph_feedback].set_type(ct_percent_bipolar);

    fxdata->p[ph_tone].set_name("Tone");
    fxdata->p[ph_tone].set_type(ct_percent_bipolar_deactivatable);

    fxdata->p[ph_width].set_name("Width");
    fxdata->p[ph_width].set_type(ct_decibel_narrow);
    fxdata->p[ph_mix].set_name("Mix");
    fxdata->p[ph_mix].set_type(ct_percent);

    fxdata->p[ph_mod_wave].posy_offset = -19;
    fxdata->p[ph_mod_rate].posy_offset = -3;
    fxdata->p[ph_mod_depth].posy_offset = -3;
    fxdata->p[ph_stereo].posy_offset = -3;

    fxdata->p[ph_stages].posy_offset = -5;
    fxdata->p[ph_center].posy_offset = 15;
    fxdata->p[ph_spread].posy_offset = -5;
    fxdata->p[ph_sharpness].posy_offset = 13;
    fxdata->p[ph_feedback].posy_offset = 17;

    fxdata->p[ph_tone].posy_offset = 1;

    fxdata->p[ph_width].posy_offset = 13;
    fxdata->p[ph_mix].posy_offset = 17;

    fxdata->p[ph_spread].dynamicDeactivation = &phGroupDeact;
}

void PhaserEffect::init_default_values()
{
    fxdata->p[ph_stages].val.i = 4;
    fxdata->p[ph_width].val.f = 0.f;
    fxdata->p[ph_spread].val.f = 0.f;
    fxdata->p[ph_mod_wave].val.i = 1;
    fxdata->p[ph_tone].val.f = 0.f;
    fxdata->p[ph_tone].deactivated = false;
    fxdata->p[ph_mod_rate].deactivated = false;
}

void PhaserEffect::handleStreamingMismatches(int streamingRevision,
                                             int currentSynthStreamingRevision)
{
    if (streamingRevision <= 13)
    {
        fxdata->p[ph_stages].val.i = 4;
        fxdata->p[ph_width].val.f = 0.f;
    }

    if (streamingRevision <= 15)
    {
        fxdata->p[ph_mod_wave].val.i = 1;
        fxdata->p[ph_mod_rate].deactivated = false;
    }

    if (streamingRevision <= 17)
    {
        fxdata->p[ph_tone].val.f = 0.f;
        fxdata->p[ph_tone].deactivated = true;
    }
}

int PhaserEffect::get_ringout_decay()
{
    auto fb = *f[ph_feedback];

    // The ringout is longer at higher feedback. This is just a heuristic based on
    // testing with the patch in #2663. Note that at feedback above 1 (from
    // modulation or control pushes) you can get infinite self-oscillation
    // so run forever then

    if (fb > 1 || fb < -1)
    {
        return -1;
    }

    if (fb > 0.9 || fb < -0.9)
    {
        return 5000;
    }

    if (fb > 0.5 || fb < -0.5)
    {
        return 3000;
    }

    return 1000;
}
