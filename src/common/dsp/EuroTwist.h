/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2021 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#ifndef SURGE_EUROTWIST_H
#define SURGE_EUROTWIST_H

#include "OscillatorBase.h"
#include <memory>
#include <memory>
#include "basic_dsp.h"

namespace plaits
{
class Voice;
class Patch;
} // namespace plaits

namespace stmlib
{
class BufferAllocator;
}

struct SRC_STATE_tag;

class EuroTwist : public Oscillator
{
  public:
    enum ParamSlots
    {
        et_engine,
        et_harmonics,
        et_timbre,
        et_morph,
        et_aux_mix,
        et_lpg_response,
        et_lpg_decay
    };
    EuroTwist(SurgeStorage *storage, OscillatorStorage *oscdata, pdata *localcopy);
    ~EuroTwist();

    void process_block(float pitch, float drift, bool stereo, bool FM, float FMdepth) override;

    virtual void init(float pitch, bool is_display = false, bool nonzero_drift = true) override;
    virtual void init_ctrltypes(int scene, int oscnum) override { init_ctrltypes(); };
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;

    // The value of the localcopy
    inline float fv(ParamSlots ps) { return localcopy[oscdata->p[ps].param_id_in_scene].f; }
    inline float fvbp(ParamSlots ps)
    {
        return limit_range((localcopy[oscdata->p[ps].param_id_in_scene].f + 1) * 0.5f, 0.f, 1.f);
    }

    std::unique_ptr<plaits::Voice> voice;
    std::unique_ptr<plaits::Patch> patch;
    std::unique_ptr<stmlib::BufferAllocator> alloc;
    char shared_buffer[16834];
    SRC_STATE_tag *srcstate;
    float carryover[BLOCK_SIZE_OS];
    int carrover_size = 0;
};

#endif // SURGE_EUROTWIST_H
