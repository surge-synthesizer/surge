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

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CONVOLUTIONEFFECT_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CONVOLUTIONEFFECT_H

#include "Effect.h"
#include <filters/BiquadFilter.h>
#include <sst/basic-blocks/dsp/SSESincDelayLine.h>
#include <vembertech/lipol.h>

#include <fft_convolve.hpp>

class ConvolutionEffect : public Effect
{
  public:
    enum convolution_params
    {
        convolution_delay = 0,
        convolution_size = 1,
        convolution_tilt_center = 2,
        convolution_tilt_slope = 3,
        convolution_locut_freq = 4,
        convolution_hicut_freq = 5,
        convolution_mix = 6,
    };

    ConvolutionEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);

    const char *get_effectname() override;
    const char *group_label(int id) override;
    int group_label_ypos(int id) override;
    void init() override;
    void init_ctrltypes() override;
    void init_default_values() override;
    void process(float *dataL, float *dataR) override;
    void updateAfterReload() override;

  private:
    void prep_ir();
    void set_params();

    bool initialized_;
    pffft::TwoStageConvolver convolverL_;
    pffft::TwoStageConvolver convolverR_;
    pffft::TwoStageConvolver xconvolver_L_;  // fade-in
    pffft::TwoStageConvolver xconvolver_R_;  // fade-in
    alignas(16) std::array<float, BLOCK_SIZE> workL_;
    alignas(16) std::array<float, BLOCK_SIZE> workR_;
    lipol_ps_blocksz mix_;
    BiquadFilter lc_, hc_;

    using delay_t = sst::basic_blocks::dsp::SSESincDelayLine<1 << 19>;
    delay_t delayL_;
    delay_t delayR_;
    lag<float> delayTime_;

    // Stored values.
    using ir_t = sst::cpputils::DynArray<float, sst::cpputils::AlignedAllocator<float, 16>>;
    ir_t irL_;
    ir_t irR_;
    float old_samplerate_;
    float old_convolution_size_;
};

#endif  // SURGE_SRC_COMMON_DSP_EFFECTS_CONVOLUTIONEFFECT_H
