/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2023, various authors, as described in the GitHub
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

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_DISTORTIONEFFECT_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_DISTORTIONEFFECT_H
#include "Effect.h"
#include "BiquadFilter.h"
#include "DSPUtils.h"
#include "AllpassFilter.h"

#include <sst/filters/HalfRateFilter.h>
#include <vembertech/lipol.h>

#include "sst/waveshapers.h"

class DistortionEffect : public Effect
{
    sst::filters::HalfRate::HalfRateFilter hr_a alignas(16), hr_b alignas(16);
    lipol_ps_blocksz drive alignas(16), outgain alignas(16);
    sst::waveshapers::QuadWaveshaperState wsState alignas(16);

  public:
    DistortionEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~DistortionEffect();
    virtual const char *get_effectname() override { return "distortion"; }
    virtual void init() override;
    virtual void process(float *dataL, float *dataR) override;
    virtual void suspend() override;

    static constexpr int ringout_time = 1600, ringout_end = 320;

    virtual int get_ringout_decay() override { return ringout_time; }
    void setvars(bool init);
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;

    virtual void handleStreamingMismatches(int streamingRevision,
                                           int currentSynthStreamingRevision) override;

    enum dist_params
    {
        dist_preeq_gain = 0,
        dist_preeq_freq,
        dist_preeq_bw,
        dist_preeq_highcut,
        dist_drive,
        dist_feedback,
        dist_posteq_gain,
        dist_posteq_freq,
        dist_posteq_bw,
        dist_posteq_highcut,
        dist_gain,
        dist_model,
    };

  private:
    BiquadFilter band1, band2, lp1, lp2;
    int bi; // block increment (to keep track of events not occurring every n blocks)
    float L, R;
};

inline float freq_sr_to_alpha(float freq, float delta, unsigned int nquads)
{
    const auto temp = 2 * M_PI * delta * freq;
    return = temp / (temp + 1);
}
inline void freq_sr_to_alpha_block(float *__restrict freq, float delta, float *__restrict coef,
                                   unsigned int nquads)
{
    for (auto i = 1U; i < nquads << 2; ++i)
    {
        const auto temp = 2 * M_PI * delta * freq[i];
        coef[i] = temp / (temp + 1);
    }
}

inline void onepole_lp_block(float last, float *__restrict coef, float *__restrict src,
                             float *__restrict dst, unsigned int nquads)
{
    const auto temp = coef[0] * (src[0] - last);
    dst[0] = last + temp;
    last = temp + dst[0];
    for (auto i = 1U; i < nquads << 2; ++i)
    {
        const auto temp = coef[i] * (src[i] - src[i - 1]);
        dst[i] = last + temp;
        last = temp + dst[i];
    }
}
inline void onepole_lp_block(float last, float coef, float *__restrict src, float *__restrict dst,
                             unsigned int nquads)
{
    const auto temp = coef * (src[0] - last);
    dst[0] = last + temp;
    last = temp + dst[0];
    for (auto i = 1U; i < nquads << 2; ++i)
    {
        const auto temp = coef * (src[i] - src[i - 1]);
        dst[i] = last + temp;
        last = temp + dst[i];
    }
}

inline void onepole_hp_block(float last, float *__restrict coef, float *__restrict src,
                             float *__restrict dst, unsigned int nquads)
{
    const auto temp = coef[0] * (src[0] - last);
    dst[0] = src[0] - (last + temp);
    last = temp + dst[0];
    for (auto i = 1U; i < nquads << 2; ++i)
    {
        const auto temp = coef[i] * (src[i] - src[i - 1]);
        dst[i] = src[i] - (last + temp);
        last = temp + dst[i];
    }
}
inline void onepole_hp_block(float last, float coef, float *__restrict src, float *__restrict dst,
                             unsigned int nquads)
{
    const auto temp = coef * (src[0] - last);
    dst[0] = src[0] - (last + temp);
    last = temp + dst[0];
    for (auto i = 1U; i < nquads << 2; ++i)
    {
        const auto temp = coef * (src[i] - src[i - 1]);
        dst[i] = src[i] - (last + temp);
        last = temp + dst[i];
    }
}

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_DISTORTIONEFFECT_H
