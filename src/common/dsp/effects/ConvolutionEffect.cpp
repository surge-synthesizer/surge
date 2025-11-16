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
#include <samplerate.h>

static constexpr double Q = 0.707;

static sst::cpputils::DynArray<uint8_t, sst::cpputils::AlignedAllocator<std::uint8_t, 16>>
convertFromFloat(sst::cpputils::DynArray<float> &src)
{
    std::size_t size = src.size() * sizeof(float);
    std::uint8_t *srcD = reinterpret_cast<std::uint8_t *>(src.data());
    return sst::cpputils::DynArray<uint8_t, sst::cpputils::AlignedAllocator<std::uint8_t, 16>>(
        srcD, srcD + size);
}

ConvolutionEffect::ConvolutionEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd)
    : Effect(storage, fxdata, pd), initialized_(false), lc_(storage), hc_(storage)
{
    std::cout << "In constructor." << std::endl;
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
    std::cout << "In init; n_user_datas = " << fxdata->user_data.size() << std::endl;
    if (fxdata->user_data.size() < 2)
        return;

    prep_ir();

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
    if (!initialized_)
        return;

    set_params();

    if (!initialized_)
        return;

    convolverL_.process(std::span<float>(dataL, BLOCK_SIZE), workL_);
    convolverR_.process(std::span<float>(dataR, BLOCK_SIZE), workR_);
    if (!fxdata->p[convolution_locut_freq].deactivated)
        lc_.process_block(workL_.data(), workR_.data());
    if (!fxdata->p[convolution_hicut_freq].deactivated)
        hc_.process_block(workL_.data(), workR_.data());
    mix_.fade_2_blocks_inplace(dataL, workL_.data(), dataR, workR_.data(), BLOCK_SIZE_QUAD);
}

void ConvolutionEffect::prep_ir()
{
    bool s = true;
    // Get the IR ready. Resample it to match the synth sample rate, and
    // normalize it so the overall magnitude is 1.
    const float inputRate = fxdata->user_data[0].as<float>()[0];
    const float outputRate = storage->samplerate * fxdata->p[convolution_size].val.f;
    const float ratio = outputRate / inputRate;
    SRC_DATA rs;
    rs.src_ratio = ratio;

    initialized_ = false;

    // Left channel (or mono).
    auto L = fxdata->user_data[1].as<float>();
    rs.data_in = L.data();
    rs.input_frames = L.size();
    irL_.reset(std::ceil(static_cast<float>(L.size()) * ratio));
    rs.data_out = irL_.data();
    rs.output_frames = irL_.size();
    int rv = src_simple(&rs, SRC_SINC_BEST_QUALITY, 1);
    if (rv != 0)
    {
        std::cout << "Error in sample rate conversion process, not running convolution. "
                  << src_strerror(rv) << std::endl;
        return;
    }
    s = convolverL_.init(BLOCK_SIZE, 256, irL_);
    if (!s)
    {
        std::cout << "Error initializing left convolver, not running convolution." << std::endl;
        return;
    }

    if (fxdata->user_data.size() > 2)
    {
        auto R = fxdata->user_data[2].as<float>();
        rs.data_in = R.data();
        rs.input_frames = R.size();
        irR_.reset(std::ceil(static_cast<float>(R.size()) * ratio));
        rs.data_out = irR_.data();
        rs.output_frames = irR_.size();
        rv = src_simple(&rs, SRC_SINC_BEST_QUALITY, 1);
        if (rv != 0)
        {
            std::cout << "Error in sample rate conversion process for right channel, not running "
                         "convolution. "
                      << src_strerror(rv) << std::endl;
            return;
        }
        s = convolverR_.init(BLOCK_SIZE, 256, irR_);
        if (!s)
        {
            std::cout << "Error initializing right convolver, not running convolution."
                      << std::endl;
            return;
        }
    }

    initialized_ = true;
    std::cout << "Convolver initialized; data was of length "
              << fxdata->user_data[1].as<float>().size() << std::endl;

    old_samplerate_ = storage->samplerate;
    old_convolution_size_ = fxdata->p[convolution_size].val.f;
}

void ConvolutionEffect::set_params()
{
    lc_.coeff_HP(lc_.calc_omega(*pd_float[convolution_locut_freq] / 12.0), Q);
    hc_.coeff_LP(lc_.calc_omega(*pd_float[convolution_hicut_freq] / 12.0), Q);
    mix_.set_target_smoothed(*pd_float[convolution_mix]);

    // Do we need a reload for non-modulatable parameter changes?
    if (storage->samplerate != old_samplerate_ ||
        fxdata->p[convolution_size].val.f != old_convolution_size_)
    {
        prep_ir();
    }
}

void ConvolutionEffect::updateAfterReload() { std::cout << "In updateAfterReload " << std::endl; }
