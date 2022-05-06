#include "RingModulatorEffect.h"
#include "Tunings.h"
#include "SineOscillator.h"
#include "DebugHelpers.h"
#include "FastMath.h"

// http://recherche.ircam.fr/pub/dafx11/Papers/66_e.pdf

RingModulatorEffect::RingModulatorEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd), halfbandIN(6, true), halfbandOUT(6, true), lp(storage),
      hp(storage)
{
    mix.set_blocksize(BLOCK_SIZE);
}

RingModulatorEffect::~RingModulatorEffect() {}

void RingModulatorEffect::init() { setvars(true); }

void RingModulatorEffect::setvars(bool init)
{
    if (init)
    {
        last_unison = -1;
        halfbandOUT.reset();
        halfbandIN.reset();

        lp.suspend();
        hp.suspend();

        hp.coeff_HP(hp.calc_omega(*f[rm_lowcut] / 12.0), 0.707);
        hp.coeff_instantize();

        lp.coeff_LP2B(lp.calc_omega(*f[rm_highcut] / 12.0), 0.707);
        lp.coeff_instantize();

        mix.instantize();
    }
}

#define OVERSAMPLE 1

void RingModulatorEffect::process(float *dataL, float *dataR)
{
    mix.set_target_smoothed(clamp01(*f[rm_mix]));

    float wetL alignas(16)[BLOCK_SIZE], wetR alignas(16)[BLOCK_SIZE];
    float dphase[MAX_UNISON];
    auto uni = std::max(1, *pdata_ival[rm_unison_voices]);

    // Has unison reset? If so modify settings
    if (uni != last_unison)
    {
        last_unison = uni;
        if (uni == 1)
        {
            detune_offset[0] = 0;
            panL[0] = 1.f;
            panR[0] = 1.f;
            phase[0] = 0.f;
        }
        else
        {
            float detune_bias = (float)2.f / (uni - 1.f);

            for (auto u = 0; u < uni; ++u)
            {
                phase[u] = u * 1.f / (uni);
                detune_offset[u] = -1.f + detune_bias * u;

                panL[u] = u / (uni - 1.f);
                panR[u] = (uni - 1.f - u) / (uni - 1.f);
            }
        }
    }

    // gain scale based on unison
    float gscale = 0.4 + 0.6 * (1.f / sqrtf(uni));
    double sri = storage->dsamplerate_inv;
    int ub = BLOCK_SIZE;

#if OVERSAMPLE
    // Now upsample
    float dataOS alignas(16)[2][BLOCK_SIZE_OS];
    halfbandIN.process_block_U2(dataL, dataR, dataOS[0], dataOS[1]);
    sri = storage->dsamplerate_os_inv;
    ub = BLOCK_SIZE_OS;
#else
    float *dataOS[2];
    dataOS[0] = dataL;
    dataOS[1] = dataR;
#endif

    for (int u = 0; u < uni; ++u)
    {
        // need to calc this every time since carrier freq could change
        if (fxdata->p[rm_unison_detune].absolute)
        {
            dphase[u] = (storage->note_to_pitch(*f[rm_carrier_freq]) * Tunings::MIDI_0_FREQ +
                         fxdata->p[rm_unison_detune].get_extended(
                             fxdata->p[rm_unison_detune].val.f * detune_offset[u])) *
                        sri;
        }
        else
        {
            dphase[u] =
                storage->note_to_pitch(*f[rm_carrier_freq] +
                                       fxdata->p[rm_unison_detune].get_extended(
                                           fxdata->p[rm_unison_detune].val.f * detune_offset[u])) *
                Tunings::MIDI_0_FREQ * sri;
        }
    }

    for (int i = 0; i < ub; ++i)
    {
        float resL = 0, resR = 0;
        for (int u = 0; u < uni; ++u)
        {
            // TODO efficiency of course
            auto vc = SineOscillator::valueFromSinAndCos(
                Surge::DSP::fastsin(2.0 * M_PI * (phase[u] - 0.5)),
                Surge::DSP::fastcos(2.0 * M_PI * (phase[u] - 0.5)), *pdata_ival[rm_carrier_shape]);
            phase[u] += dphase[u];

            if (phase[u] > 1)
            {
                phase[u] -= (int)phase[u];
            }

            for (int c = 0; c < 2; ++c)
            {
                auto vin = (c == 0 ? dataOS[0][i] : dataOS[1][i]);
                auto wd = 1.0;

                auto A = 0.5 * vin + vc;
                auto B = vc - 0.5 * vin;

                float dPA = diode_sim(A);
                float dMA = diode_sim(-A);
                float dPB = diode_sim(B);
                float dMB = diode_sim(-B);

                float res = dPA + dMA - dPB - dMB;
                resL += res * panL[u];
                resR += res * panR[u];
            }
        }

        auto outl = gscale * resL;
        auto outr = gscale * resR;

        outl = 1.5 * outl - 0.5 * outl * outl * outl;
        outr = 1.5 * outr - 0.5 * outr * outr * outr;

        dataOS[0][i] = outl;
        dataOS[1][i] = outr;
    }

#if OVERSAMPLE
    halfbandOUT.process_block_D2(dataOS[0], dataOS[1]);
    copy_block(dataOS[0], wetL, BLOCK_SIZE_QUAD);
    copy_block(dataOS[1], wetR, BLOCK_SIZE_QUAD);
#endif

    // Apply the filters
    hp.coeff_HP(hp.calc_omega(*f[rm_lowcut] / 12.0), 0.707);
    lp.coeff_LP2B(lp.calc_omega(*f[rm_highcut] / 12.0), 0.707);

    if (!fxdata->p[rm_highcut].deactivated)
    {
        lp.process_block(wetL, wetR);
    }

    if (!fxdata->p[rm_lowcut].deactivated)
    {
        hp.process_block(wetL, wetR);
    }

    mix.fade_2_blocks_to(dataL, wetL, dataR, wetR, dataL, dataR, BLOCK_SIZE_QUAD);
}

void RingModulatorEffect::suspend() { init(); }

const char *RingModulatorEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Carrier";
    case 1:
        return "Diode";
    case 2:
        return "EQ";
    case 3:
        return "Output";
    }
    return 0;
}

int RingModulatorEffect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 11;
    case 2:
        return 17;
    case 3:
        return 23;
    }
    return 0;
}

void RingModulatorEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[rm_carrier_shape].set_name("Shape");
    fxdata->p[rm_carrier_shape].set_type(ct_sineoscmode);
    fxdata->p[rm_carrier_freq].set_name("Frequency");
    fxdata->p[rm_carrier_freq].set_type(ct_freq_ringmod);
    fxdata->p[rm_unison_detune].set_name("Unison Detune");
    fxdata->p[rm_unison_detune].set_type(ct_oscspread);
    fxdata->p[rm_unison_voices].set_name("Unison Voices");
    fxdata->p[rm_unison_voices].set_type(ct_osccount);

    fxdata->p[rm_diode_fwdbias].set_name("Forward Bias");
    fxdata->p[rm_diode_fwdbias].set_type(ct_percent);
    fxdata->p[rm_diode_linregion].set_name("Linear Region");
    fxdata->p[rm_diode_linregion].set_type(ct_percent);

    fxdata->p[rm_lowcut].set_name("Low Cut");
    fxdata->p[rm_lowcut].set_type(ct_freq_audible_deactivatable_hp);
    fxdata->p[rm_highcut].set_name("High Cut");
    fxdata->p[rm_highcut].set_type(ct_freq_audible_deactivatable_lp);

    fxdata->p[rm_mix].set_name("Mix");
    fxdata->p[rm_mix].set_type(ct_percent);

    for (int i = rm_carrier_shape; i < rm_num_params; ++i)
    {
        auto a = 1;
        if (i >= rm_diode_fwdbias)
            a += 2;
        if (i >= rm_lowcut)
            a += 2;
        if (i >= rm_mix)
            a += 2;
        fxdata->p[i].posy_offset = a;
    }
}

void RingModulatorEffect::init_default_values()
{
    fxdata->p[rm_carrier_freq].val.f = 60;
    fxdata->p[rm_carrier_shape].val.i = 0;
    fxdata->p[rm_diode_fwdbias].val.f = 0.3;
    fxdata->p[rm_diode_linregion].val.f = 0.7;
    fxdata->p[rm_unison_detune].val.f = 0.2;
    fxdata->p[rm_unison_voices].val.i = 1;

    fxdata->p[rm_lowcut].val.f = fxdata->p[rm_lowcut].val_min.f;
    fxdata->p[rm_lowcut].deactivated = false;

    fxdata->p[rm_highcut].val.f = fxdata->p[rm_highcut].val_max.f;
    fxdata->p[rm_highcut].deactivated = false;
    fxdata->p[rm_mix].val.f = 1.0;
}

void RingModulatorEffect::handleStreamingMismatches(int streamingRevision,
                                                    int currentSynthStreamingRevision)
{
    if (streamingRevision <= 15)
    {
        fxdata->p[rm_lowcut].deactivated = false;
        fxdata->p[rm_highcut].deactivated = false;
    }
}

float RingModulatorEffect::diode_sim(float v)
{
    auto vb = *(f[rm_diode_fwdbias]);
    auto vl = *(f[rm_diode_linregion]);
    auto h = 1.f;
    vl = std::max(vl, vb + 0.02f);
    if (v < vb)
    {
        return 0;
    }
    if (v < vl)
    {
        auto vvb = v - vb;
        return h * vvb * vvb / (2.f * vl - 2.f * vb);
    }
    auto vlvb = vl - vb;
    return h * v - h * vl + h * vlvb * vlvb / (2.f * vl - 2.f * vb);
}
