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
#include <sst/filters/CytomicTilt.h>
#include <vembertech/lipol.h>

#include "ConvolutionKernel.h"

class ConvolutionEffect : public Effect
{
  public:
    static constexpr float minimumSize = .5f, maximumSize = 2.f;
    static constexpr float maximumStart = .9f, maximumReverse = .5f;
    enum convolution_params
    {
        convolution_delay = 0,
        convolution_size = 1,
        convolution_start = 2,
        convolution_reverse = 3,
        convolution_tilt_center = 4,
        convolution_tilt_slope = 5,
        convolution_locut_freq = 6,
        convolution_hicut_freq = 7,
        convolution_mix = 8,
    };

    bool initialized;

    ConvolutionEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    ~ConvolutionEffect() override;

    const char *get_effectname() override;
    const char *group_label(int id) override;
    int group_label_ypos(int id) override;

    void init() override;
    void init_ctrltypes() override;
    void init_default_values() override;
    int get_ringout_decay() override;
    void process(float *dataL, float *dataR) override;
    void rebindParameterStorage(FxStorage *parameters, pdata *values) override;
    // Control-thread preparation without decoding/resampling/building an IR.
    void prepareProcessingState();
    // Engine owner only. The fresh replacement must not own an old kernel.
    void adoptPreparedKernel(std::unique_ptr<ConvolutionKernel> &prepared)
    {
        assert(!kernel_);
        kernel_.swap(prepared);
        initialized = bool(kernel_);
        rememberKernelSettings();
    }
#if SURGE_WEB
    // Engine owner only, under fxSpawnMutex immediately before effect replacement.
    std::unique_ptr<ConvolutionKernel> &browserKernel() { return kernel_; }
    void browserDetachForRetirement();
#endif

  private:
    void prep_ir();
    void set_params();
    void rememberKernelSettings();
#if SURGE_WEB
    void updateBrowserKernel();
    int browserSlot_{-1};
    uint64_t browserRequest_{0};
    float requestedRate_{}, requestedSize_{}, requestedStart_{}, requestedReverse_{};
    bool requestedReverseDeactivated_{};
#endif

    std::unique_ptr<ConvolutionKernel> kernel_;
    alignas(16) std::array<float, BLOCK_SIZE> workL_;
    alignas(16) std::array<float, BLOCK_SIZE> workR_;
    alignas(16) std::array<float, BLOCK_SIZE> delayedL_;
    alignas(16) std::array<float, BLOCK_SIZE> delayedR_;
    lipol_ps_blocksz mix_;
    BiquadFilter lc_, hc_;
    sst::filters::CytomicTilt tilt_;

    using delay_t = sst::basic_blocks::dsp::SSESincDelayLine<1 << 19>;
    delay_t delayL_;
    delay_t delayR_;
    lag<float> delayTime_;
    bool delay_latched_;

    // Stored values.
    float old_samplerate_;
    float old_convolution_size_;
    float old_start_;
    float old_reverse_;
    bool old_reverse_deactivated_;
};

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CONVOLUTIONEFFECT_H
