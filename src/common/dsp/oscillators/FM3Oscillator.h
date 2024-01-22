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

#ifndef SURGE_SRC_COMMON_DSP_OSCILLATORS_FM3OSCILLATOR_H
#define SURGE_SRC_COMMON_DSP_OSCILLATORS_FM3OSCILLATOR_H

#include "OscillatorBase.h"
#include "DSPUtils.h"
#include <vembertech/lipol.h>
#include "BiquadFilter.h"
#include "OscillatorCommonFunctions.h"
#include "sst/basic-blocks/dsp/QuadratureOscillators.h"

class FM3Oscillator : public Oscillator
{
  public:
    enum fm3_params
    {
        fm3_m1amount = 0,
        fm3_m1ratio,
        fm3_m2amount,
        fm3_m2ratio,
        fm3_m3amount,
        fm3_m3freq,
        fm3_feedback,
    };

    FM3Oscillator(SurgeStorage *storage, OscillatorStorage *oscdata, pdata *localcopy);
    virtual void init(float pitch, bool is_display = false,
                      bool nonzero_init_drift = true) override;
    virtual void process_block(float pitch, float drift = 0.f, bool stereo = false, bool FM = false,
                               float FMdepth = 0.f) override;
    virtual ~FM3Oscillator();
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;

    template <int mode, bool stereo, bool FM>
    void process_block_internal(float pitch, float drift, float FMdepth);

    double phase, oldout1, oldout2;

    using quadr_osc = sst::basic_blocks::dsp::SurgeQuadrOsc<float>;

    quadr_osc RM1, RM2, AM;
    Surge::Oscillator::DriftLFO driftLFO;
    float fb_val;
    int fb_mode;
    lag<double> FMdepth, AbsModDepth, RelModDepth1, RelModDepth2, FeedbackDepth;
    virtual void handleStreamingMismatches(int streamingRevision,
                                           int currentSynthStreamingRevision) override;
};

#endif // SURGE_SRC_COMMON_DSP_OSCILLATORS_FM3OSCILLATOR_H
