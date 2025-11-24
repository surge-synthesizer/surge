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
#include "ConvolutionEffect.h"
#include <iostream>

#include <CDSPResampler.h>

static constexpr double Q = 0.707;

ConvolutionEffect::ConvolutionEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd), initialized_(false), lc_(storage), hc_(storage),
      delayL_(storage->sinctable), delayR_(storage->sinctable), delayTime_(.001)
{
    mix_.set_blocksize(BLOCK_SIZE);
}

const char *ConvolutionEffect::get_effectname() { return "convolution"; }

const char *ConvolutionEffect::group_label(int id)
{
    switch (id)
    {
    case 0:
        return "Input";
    case 1:
        return "Settings";
    }

    return nullptr;
}

int ConvolutionEffect::group_label_ypos(int id)
{
    switch (id)
    {
    case 0:
        return 1;
    case 1:
        return 3;
    }
    return 0;
}

void ConvolutionEffect::init()
{
    if (!(fxdata->user_data.contains("irname") && fxdata->user_data.contains("samplerate") &&
          fxdata->user_data.contains("left")))
        return;

    prep_ir();

    lc_.coeff_HP(lc_.calc_omega(*pd_float[convolution_locut_freq] / 12.0), Q);
    hc_.coeff_LP(lc_.calc_omega(*pd_float[convolution_hicut_freq] / 12.0), Q);
    lc_.coeff_instantize();
    hc_.coeff_instantize();
    tilt_.setCoeff(*pd_float[convolution_tilt_center], 0, storage->samplerate_inv,
                   storage->db_to_linear(*pd_float[convolution_tilt_slope] * 0.5f));
    mix_.set_target(fxdata->p[convolution_mix].val.f);
}

void ConvolutionEffect::init_ctrltypes()
{
    Effect::init_ctrltypes();

    fxdata->p[convolution_delay].set_name("Delay");
    fxdata->p[convolution_delay].set_type(ct_convolution_delay);
    fxdata->p[convolution_delay].val_min.f = 0.f;
    fxdata->p[convolution_delay].val_max.f = 1.f;
    fxdata->p[convolution_delay].val_default.f = 0.f;
    fxdata->p[convolution_delay].posy_offset = 3;

    fxdata->p[convolution_size].set_name("Size");
    fxdata->p[convolution_size].set_type(ct_percent);
    fxdata->p[convolution_size].val_min.f = 0.5f;
    fxdata->p[convolution_size].val_max.f = 2.f;
    fxdata->p[convolution_size].val_default.f = 1.f;
    fxdata->p[convolution_size].posy_offset = 3;

    fxdata->p[convolution_tilt_center].set_name("Tilt Center");
    fxdata->p[convolution_tilt_center].set_type(ct_freq_audible_deactivatable);
    fxdata->p[convolution_tilt_center].set_extend_range(false);
    fxdata->p[convolution_tilt_center].posy_offset = 3;
    fxdata->p[convolution_tilt_slope].set_name("Tilt Slope");
    fxdata->p[convolution_tilt_slope].set_type(ct_decibel_narrow_deactivatable);
    fxdata->p[convolution_tilt_slope].val_min.f = -18.;
    fxdata->p[convolution_tilt_slope].val_max.f = 18.;
    fxdata->p[convolution_tilt_slope].val_default.f = 0.f;
    fxdata->p[convolution_tilt_slope].posy_offset = 3;

    fxdata->p[convolution_locut_freq].set_name("Low Cut");
    fxdata->p[convolution_locut_freq].set_type(ct_freq_audible_deactivatable_hp);
    fxdata->p[convolution_locut_freq].posy_offset = 3;
    fxdata->p[convolution_hicut_freq].set_name("High Cut");
    fxdata->p[convolution_hicut_freq].set_type(ct_freq_audible_deactivatable_lp);
    fxdata->p[convolution_hicut_freq].posy_offset = 3;

    fxdata->p[convolution_mix].set_name("Mix");
    fxdata->p[convolution_mix].set_type(ct_percent);
    fxdata->p[convolution_mix].val_default.f = 1.f;
    fxdata->p[convolution_mix].posy_offset = 3;
}

void ConvolutionEffect::init_default_values()
{
    fxdata->p[convolution_delay].val.f = 0.f;
    fxdata->p[convolution_size].val.f = 1.f;
    fxdata->p[convolution_tilt_center].deactivated = true;
    fxdata->p[convolution_tilt_slope].val.f = 0.f;
    fxdata->p[convolution_tilt_slope].deactivated = true;
    fxdata->p[convolution_locut_freq].val.f = -3.f * 12.f;
    fxdata->p[convolution_locut_freq].deactivated = true;
    fxdata->p[convolution_hicut_freq].val.f = -3.f * 12.f;
    fxdata->p[convolution_hicut_freq].deactivated = true;
    fxdata->p[convolution_mix].val.f = 1.f;
}

void ConvolutionEffect::process(float *dataL, float *dataR)
{
    if (!initialized_)
        return;

    set_params();

    if (!initialized_)
        return;

    for (std::size_t i = 0; i < BLOCK_SIZE; i++)
    {
        delayL_.write(dataL[i]);
        delayR_.write(dataR[i]);
    }

    // Cut out some annoying non-SSEable moves if we can.
    if (delayTime_.v != 0)
    {
        for (std::size_t i = 0; i < BLOCK_SIZE; i++)
        {
            delayedL_[i] = delayL_.readLinear(delayTime_.v + (BLOCK_SIZE - 1 - i));
            delayedR_[i] = delayR_.readLinear(delayTime_.v + (BLOCK_SIZE - 1 - i));
        }
    }
    else
    {
        std::copy(dataL, dataL + BLOCK_SIZE, delayedL_.begin());
        std::copy(dataR, dataR + BLOCK_SIZE, delayedR_.begin());
    }

    convolverL_.process(delayedL_, workL_);
    convolverR_.process(delayedR_, workR_);
    if (!fxdata->p[convolution_tilt_center].deactivated)
        tilt_.processBlock<BLOCK_SIZE>(workL_.data(), workR_.data());
    if (!fxdata->p[convolution_locut_freq].deactivated)
        lc_.process_block(workL_.data(), workR_.data());
    if (!fxdata->p[convolution_hicut_freq].deactivated)
        hc_.process_block(workL_.data(), workR_.data());
    mix_.fade_2_blocks_inplace(dataL, workL_.data(), dataR, workR_.data());
}

void ConvolutionEffect::prep_ir()
{
    bool s = true;
    // Get the IR ready. Resample it to match the synth sample rate, and
    // normalize it so the overall magnitude is 1.
    const float inputRate = fxdata->by_key("samplerate").to_float();
    const float outputRate = storage->samplerate * fxdata->p[convolution_size].val.f;
    const float ratio = outputRate / inputRate;

    // Left channel (or mono).
    auto L = fxdata->by_key("left").as<float>();

    r8b::CDSPResampler resampler(inputRate, outputRate, L.size());
    irL_.reset(resampler.getMaxOutLen(0));
    irR_.reset(resampler.getMaxOutLen(0));

    // Resampler needs it in double-precision.
    sst::cpputils::DynArray<double> in(L.size()), out(resampler.getMaxOutLen(0));
    std::copy(L.begin(), L.end(), in.begin());

    resampler.oneshot(in.data(), in.size(), out.data(), out.size());
    std::copy(out.begin(), out.end(), irL_.begin());

    irR_.reset(out.size());
    if (fxdata->user_data.contains("right"))
    {
        auto R = fxdata->by_key("right").as<float>();
        std::copy(R.begin(), R.end(), in.begin());
        resampler.oneshot(in.data(), in.size(), out.data(), out.size());
        std::copy(out.begin(), out.end(), irR_.begin());
    }
    else
    {
        irR_ = irL_;
    }

    s = s && convolverL_.init(BLOCK_SIZE, 256, irL_);
    s = s && convolverR_.init(BLOCK_SIZE, 256, irR_);
    if (!s)
    {
        std::cerr << "Error initializing convolver, not running convolution." << std::endl;
        return;
    }

    initialized_ = true;
    old_samplerate_ = storage->samplerate;
    old_convolution_size_ = fxdata->p[convolution_size].val.f;
}

void ConvolutionEffect::set_params()
{
    lc_.coeff_HP(lc_.calc_omega(*pd_float[convolution_locut_freq] / 12.0), Q);
    hc_.coeff_LP(lc_.calc_omega(*pd_float[convolution_hicut_freq] / 12.0), Q);
    tilt_.setCoeffForBlock<BLOCK_SIZE>(
        *pd_float[convolution_tilt_center], 0, storage->samplerate_inv,
        storage->db_to_linear(*pd_float[convolution_tilt_slope] * 0.5f));
    mix_.set_target_smoothed(*pd_float[convolution_mix]);
    delayTime_.newValue(storage->samplerate * *pd_float[convolution_delay]);

    // Do we need a reload for non-modulatable parameter changes?
    if (storage->samplerate != old_samplerate_ ||
        fxdata->p[convolution_size].val.f != old_convolution_size_)
    {
        prep_ir();
    }
}
