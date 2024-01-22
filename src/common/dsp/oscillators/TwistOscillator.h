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

/*
 * What's our samplerate strategy
 */
#include "globals.h"
#define SAMPLERATE_SRC 0
#define SAMPLERATE_LANCZOS 1

#include "OscillatorBase.h"
#include <memory>
#include "basic_dsp.h"
#include "DSPUtils.h"
#include "OscillatorCommonFunctions.h"

#if SAMPLERATE_LANCZOS
// #include "LanczosResampler.h"
#include "sst/basic-blocks/dsp/LanczosResampler.h"
#endif

namespace plaits
{
class Voice;
class Patch;
class Modulations;
} // namespace plaits

namespace stmlib
{
class BufferAllocator;
}

struct SRC_STATE_tag;

class TwistOscillator : public Oscillator
{
  public:
    enum twist_params
    {
        twist_engine,
        twist_harmonics,
        twist_timbre,
        twist_morph,
        twist_aux_mix,
        twist_lpg_response,
        twist_lpg_decay
    };

    TwistOscillator(SurgeStorage *storage, OscillatorStorage *oscdata, pdata *localcopy);
    ~TwistOscillator();

    void process_block(float pitch, float drift, bool stereo, bool FM, float FMdepth) override;

    template <bool FM, bool throwaway = false>
    void process_block_internal(float pitch, float drift, bool stereo, float FMdepth,
                                int throwawayBlocks = -1);

    virtual void init(float pitch, bool is_display = false, bool nonzero_drift = true) override;
    virtual void init_ctrltypes(int scene, int oscnum) override { init_ctrltypes(); };
    virtual void init_ctrltypes() override;
    virtual void init_default_values() override;
    float tuningAwarePitch(float pitch);

    // The value of the localcopy
    inline float fv(twist_params ps) { return localcopy[oscdata->p[ps].param_id_in_scene].f; }
    inline float fvbp(twist_params ps)
    {
        return clamp01((localcopy[oscdata->p[ps].param_id_in_scene].f + 1) * 0.5f);
    }

    std::unique_ptr<plaits::Voice> voice;
    std::unique_ptr<plaits::Patch> patch;
    std::unique_ptr<plaits::Modulations> mod;
    std::unique_ptr<stmlib::BufferAllocator> alloc;
    char *shared_buffer{nullptr};

    // Keep this here for now even if using lanczos since I'm using SRC for FM still
    SRC_STATE_tag *srcstate, *fmdownsamplestate;
    float fmlagbuffer[BLOCK_SIZE_OS << 1];
    int fmwp, fmrp;

    bool useCorrectLPGBlockSize{false}; // See #6760

#if SAMPLERATE_LANCZOS
    std::unique_ptr<sst::basic_blocks::dsp::LanczosResampler<BLOCK_SIZE>> lancRes;
#endif

    float carryover[BLOCK_SIZE_OS][2];
    int carrover_size = 0;

    lag<float, true> harm, timb, morph, lpgcol, lpgdec, auxmix;

    Surge::Oscillator::DriftLFO driftLFO;
    Surge::Oscillator::CharacterFilter<float> charFilt;
};
