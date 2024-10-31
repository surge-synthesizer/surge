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

#ifndef SURGE_SRC_COMMON_DSP_EFFECT_H
#define SURGE_SRC_COMMON_DSP_EFFECT_H

#include "globals.h"
#include "DSPUtils.h"
#include "SurgeStorage.h"
#include "lipol.h"

#include "sst/basic-blocks/dsp/MidSide.h"
#include "sst/basic-blocks/dsp/Lag.h"

/*	base class			*/

namespace surge::sstfx
{
struct SurgeFXConfig;
}

class alignas(16) Effect
{
  public:
    template <typename T, bool first = true> using lag = sst::basic_blocks::dsp::SurgeLag<T, first>;

    enum
    {
        KNumVuSlots = 24
    };

    Effect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~Effect() { return; }

    virtual const char *get_effectname() { return 0; }

    virtual void init(){};
    virtual void init_ctrltypes();
    virtual void init_default_values(){};

    // No matter what path is used to reload (whether created anew or what not) this is called after
    // the loading state of an item has changed
    virtual void updateAfterReload(){};
    virtual Surge::ParamConfig::VUType vu_type(int id) { return Surge::ParamConfig::vut_off; };
    virtual int vu_ypos(int id) { return id; }; // in 'half-hslider' heights
    virtual const char *group_label(int id) { return 0; };
    virtual int group_label_ypos(int id) { return 0; };
    virtual int get_ringout_decay()
    {
        return -1;
    } // number of blocks it takes for the effect to 'ring out'
    int groupIndexForParamIndex(int paramIndex)
    {
        int fpos = fxdata->p[paramIndex].posy / 10 + fxdata->p[paramIndex].posy_offset;
        int res = -1;
        for (auto j = 0; j < n_fx_params && group_label(j); ++j)
        {
            if (group_label(j) && group_label_ypos(j) <= fpos // constants for SurgeGUIEditor. Sigh.
            )
            {
                res = j;
            }
        }
        return res;
    }

    virtual void process(float *dataL, float *dataR) { return; }
    virtual void process_only_control()
    {
        return;
    } // for controllers that should run regardless of the audioprocess
    virtual bool process_ringout(float *dataL, float *dataR,
                                 bool indata_present = true); // returns rtue if outdata is present
    // virtual void processSSE(float *dataL, float *dataR){ return; }
    // virtual void processSSE2(float *dataL, float *dataR){ return; }
    // virtual void processSSE3(float *dataL, float *dataR){ return; }
    // virtual void processT<int architecture>(float *dataL, float *dataR){ return; }
    virtual void suspend() { return; }
    float vu[KNumVuSlots]; // stereo pairs, just use every other when mono

    // Most of the fx read the sample rate at sample time but airwindows
    // keeps a cache so give loaded fx a notice when the sample rate changes
    virtual void sampleRateReset() {}

    virtual void handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision)
    {
        // No-op here.
    }

    inline bool checkHasInvalidatedUI()
    {
        auto x = hasInvalidated;
        hasInvalidated = false;
        return x;
    }

    inline void applyWidth(float *__restrict L, float *__restrict R, lipol_ps_blocksz &width)
    {
        namespace sdsp = sst::basic_blocks::dsp;
        float M alignas(16)[BLOCK_SIZE], S alignas(16)[BLOCK_SIZE];
        sdsp::encodeMS<BLOCK_SIZE>(L, R, M, S);
        width.multiply_block(S, BLOCK_SIZE_QUAD);
        sdsp::decodeMS<BLOCK_SIZE>(M, S, L, R);
    }

    float *pd_float[n_fx_params];
    int *pd_int[n_fx_params];

    friend struct surge::sstfx::SurgeFXConfig;

  protected:
    SurgeStorage *storage;
    FxStorage *fxdata;
    pdata *pd;
    int ringout;
    bool hasInvalidated{false};
};

// Some common constants
const int max_delay_length = 1 << 18;
const int slowrate = 8;
const int slowrate_m1 = slowrate - 1;

Effect *spawn_effect(int id, SurgeStorage *storage, FxStorage *fxdata, pdata *pd);

#endif // SURGE_SRC_COMMON_DSP_EFFECT_H
