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

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_RINGMODULATOREFFECT_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_RINGMODULATOREFFECT_H
#include "Effect.h"
#include "BiquadFilter.h"
#include "DSPUtils.h"

#include <vembertech/lipol.h>
#include <sst/filters/HalfRateFilter.h>

class RingModulatorEffect : public Effect
{
  public:
    static constexpr int MAX_UNISON = 16;

    RingModulatorEffect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~RingModulatorEffect();
    virtual const char *get_effectname() override { return "ringmodulator"; }
    virtual void init() override;
    virtual void process(float *dataL, float *dataR) override;
    virtual void suspend() override;
    void setvars(bool init);
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;

    virtual const char *group_label(int id) override;
    virtual int group_label_ypos(int id) override;

    virtual int get_ringout_decay() override { return ringout_value; }

    virtual void handleStreamingMismatches(int streamingRevision,
                                           int currentSynthStreamingRevision) override;

    float diode_sim(float x);

    enum ringmod_params
    {
        rm_carrier_shape = 0,
        rm_carrier_freq,
        rm_unison_detune,
        rm_unison_voices,

        rm_diode_fwdbias,
        rm_diode_linregion,

        rm_lowcut,
        rm_highcut,

        rm_mix,

        rm_num_params,
    };

  private:
    int ringout_value = -1;
    float phase[MAX_UNISON], detune_offset[MAX_UNISON], panL[MAX_UNISON], panR[MAX_UNISON];
    int last_unison = -1;

    sst::filters::HalfRate::HalfRateFilter halfbandOUT, halfbandIN;
    BiquadFilter lp, hp;
    lipol_ps_blocksz mix alignas(16);
};

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_RINGMODULATOREFFECT_H
