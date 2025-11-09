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

static constexpr double Q = 0.707;

ConvolutionEffect::ConvolutionEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd), initialized_(false), lc_(storage), hc_(storage)
{
    std::cout << "In constructor." << std::endl;
    mix_.set_blocksize(BLOCK_SIZE);
}

const char *ConvolutionEffect::get_effectname() { return "convolution"; }

const char *ConvolutionEffect::group_label(int id)
{
    switch (id) {
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
    std::cout << "In init; n_user_datas = " << fxdata->user_data.size() << std::endl;
    if (fxdata->user_data.size() < 2)
        return;

    bool s = true;
    // We're reloaded when we get a new IR.
    std::span<float> data = fxdata->user_data[1].as<float>();
    s = convolverL_.init(BLOCK_SIZE, 256, data);
    if (!s)
        std::cout << "Error initializing left convolver." << std::endl;
    if (fxdata->user_data.size() > 2)
        data = fxdata->user_data[2].as<float>();
    s = convolverR_.init(BLOCK_SIZE, 256, data);
    if (!s)
        std::cout << "Error initializing right convolver." << std::endl;
    initialized_ = s;
    std::cout << "Convolver initialized; data was of length "
              << fxdata->user_data[1].as<float>().size() << std::endl;

    lc_.coeff_HP(lc_.calc_omega(*pd_float[convolution_locut_freq] / 12.0), Q);
    hc_.coeff_LP(lc_.calc_omega(*pd_float[convolution_hicut_freq] / 12.0), Q);
    lc_.coeff_instantize();
    hc_.coeff_instantize();
    mix_.set_target(fxdata->p[convolution_mix].val.f);
}

void ConvolutionEffect::init_ctrltypes()
{
    std::cout << "In init_ctrltypes." << std::endl;
    Effect::init_ctrltypes();

    fxdata->p[convolution_delay].set_name("Delay");
    fxdata->p[convolution_delay].set_type(ct_envtime);
    fxdata->p[convolution_delay].posy_offset = 3;

    fxdata->p[convolution_size].set_name("Size");
    fxdata->p[convolution_size].set_type(ct_percent);
    fxdata->p[convolution_size].val_min.f = 0.5f;
    fxdata->p[convolution_size].val_max.f = 2.f;
    fxdata->p[convolution_size].posy_offset = 3;

    fxdata->p[convolution_tilt_center].set_name("Tilt Center");
    fxdata->p[convolution_tilt_center].set_type(ct_freq_audible_deactivatable);
    fxdata->p[convolution_tilt_center].set_extend_range(false);
    fxdata->p[convolution_tilt_center].posy_offset = 3;
    fxdata->p[convolution_tilt_slope].set_name("Tilt Slope");
    fxdata->p[convolution_tilt_slope].set_type(ct_decibel_narrow_deactivatable);
    fxdata->p[convolution_tilt_slope].val_min.f = -18.;
    fxdata->p[convolution_tilt_slope].val_max.f = 18.;
    fxdata->p[convolution_tilt_slope].posy_offset = 3;

    fxdata->p[convolution_locut_freq].set_name("Low Cut");
    fxdata->p[convolution_locut_freq].set_type(ct_freq_audible_deactivatable_hp);
    fxdata->p[convolution_locut_freq].posy_offset = 3;
    fxdata->p[convolution_hicut_freq].set_name("High Cut");
    fxdata->p[convolution_hicut_freq].set_type(ct_freq_audible_deactivatable_lp);
    fxdata->p[convolution_hicut_freq].posy_offset = 3;

    fxdata->p[convolution_mix].set_name("Mix");
    fxdata->p[convolution_mix].set_type(ct_percent);
    fxdata->p[convolution_mix].posy_offset = 3;
}

void ConvolutionEffect::init_default_values()
{
    std::cout << "In init_default_values." << std::endl;
    fxdata->p[convolution_delay].val.f = 0.f;
    fxdata->p[convolution_delay].val_default.f = 0.f;
    fxdata->p[convolution_size].val.f = 1.f;
    fxdata->p[convolution_size].val_default.f = 1.f;
    fxdata->p[convolution_tilt_center].deactivated = true;
    fxdata->p[convolution_tilt_slope].val.f = 0.f;
    fxdata->p[convolution_tilt_slope].val_default.f = 0.f;
    fxdata->p[convolution_tilt_slope].deactivated = true;
    fxdata->p[convolution_locut_freq].val.f = -3.f * 12.f;
    fxdata->p[convolution_locut_freq].deactivated = true;
    fxdata->p[convolution_hicut_freq].val.f = -3.f * 12.f;
    fxdata->p[convolution_hicut_freq].deactivated = true;
    fxdata->p[convolution_mix].val.f = 0.f;
}

void ConvolutionEffect::process(float *dataL, float *dataR)
{
    // std::cout << "In process" << std::endl;
    //  TODO: Need to match the sample rates between the impulse response and
    //  the current sample rate. don't do that yet here.
    if (!initialized_)
        return;

    set_params();

    convolverL_.process(std::span<float>(dataL, BLOCK_SIZE), workL_);
    convolverR_.process(std::span<float>(dataR, BLOCK_SIZE), workR_);
    if (!fxdata->p[convolution_locut_freq].deactivated)
        lc_.process_block(workL_.data(), workR_.data());
    if (!fxdata->p[convolution_hicut_freq].deactivated)
        hc_.process_block(workL_.data(), workR_.data());
    mix_.fade_2_blocks_inplace(dataL, workL_.data(), dataR, workR_.data(), BLOCK_SIZE_QUAD);
}

void ConvolutionEffect::set_params()
{
    lc_.coeff_HP(lc_.calc_omega(*pd_float[convolution_locut_freq] / 12.0), Q);
    hc_.coeff_LP(lc_.calc_omega(*pd_float[convolution_hicut_freq] / 12.0), Q);
    mix_.set_target_smoothed(*pd_float[convolution_mix]);
}

void ConvolutionEffect::updateAfterReload() { std::cout << "In updateAfterReload " << std::endl; }
