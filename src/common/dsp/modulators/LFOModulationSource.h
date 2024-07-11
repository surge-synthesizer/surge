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

#ifndef SURGE_SRC_COMMON_DSP_MODULATORS_LFOMODULATIONSOURCE_H
#define SURGE_SRC_COMMON_DSP_MODULATORS_LFOMODULATIONSOURCE_H

#include "DSPUtils.h"
#include "SurgeStorage.h"
#include "SurgeVoiceState.h"
#include "ModulationSource.h"
#include "MSEGModulationHelper.h" // We need this for the MSEGEvalatorState member
#include "FormulaModulationHelper.h"
#include <functional>
#include "sst/basic-blocks/dsp/QuadratureOscillators.h"

enum LFOEG_state
{
    lfoeg_off = 0,
    lfoeg_delay,
    lfoeg_attack,
    lfoeg_hold,
    lfoeg_decay,
    lfoeg_release,
    lfoeg_msegrelease,
    lfoeg_stuck,
};

class LFOModulationSource : public ModulationSource
{
  public:
    LFOModulationSource();
    ~LFOModulationSource();
    void assign(SurgeStorage *storage, LFOStorage *lfo, pdata *localcopy, SurgeVoiceState *state,
                StepSequencerStorage *ss, MSEGStorage *ms, FormulaModulatorStorage *fs,
                bool is_display = false);
    float bend1(float x);
    float bend2(float x);
    float bend3(float x);
    virtual void attack() override { attackFrom(0.f); }
    void attackFrom(float);
    virtual void release() override;
    virtual void process_block() override;
    virtual void retriggerEnvelope() { attackFrom(0.f); }
    virtual void retriggerEnvelopeFrom(float);
    virtual void completedModulation();

    enum EnvelopeRetriggerMode
    {
        FROM_ZERO,
        FROM_LAST
    } envRetrigMode{FROM_ZERO};

    float envelopeStart{0.f};

    int get_active_outputs() override
    {
        if (lfo->shape.val.i == lt_formula)
            return Surge::Formula::max_formula_outputs;
        else
            return 3;
    }
    float get_output(int which) const override { return output_multi[which]; }
    float get_output01(int which) const override { return output_multi[which]; }
    void set_output(int which, float f) override
    {
        // set_output on an LFO should never be called. Blow up in debug if it is
        assert(false);
        output_multi[which] = f;
    }

    bool isVoice{false};
    void setIsVoice(bool b) { isVoice = b; }

    virtual const char *get_title() override { return "LFO"; }
    virtual int get_type() override { return mst_lfo; }
    virtual bool is_bipolar() override { return true; }
    float env_val;
    int env_state;
    bool retrigger_FEG;
    bool retrigger_AEG;

    bool everAttacked{false};

  private:
    LFOStorage *lfo;
    SurgeVoiceState *state;
    SurgeStorage *storage;
    StepSequencerStorage *ss;
    MSEGStorage *ms;

  public:
    FormulaModulatorStorage *fs;

  private:
    float output_multi[Surge::Formula::max_formula_outputs];

  public:
    Surge::MSEG::EvaluatorState msegstate;
    Surge::Formula::EvaluatorState formulastate;

    inline float getPhase() { return phase; }
    inline int getIntPhase() { return unwrappedphase_intpart; }
    inline int getEnvState() { return env_state; }
    inline int getStep() { return step; }

    float onepoleFactor{0};

  private:
    pdata *localcopy;
    bool phaseInitialized;
    void initPhaseFromStartPhase();
    void msegEnvelopePhaseAdjustment();

    float phase, target, noise, noised1, env_phase, priorPhase;
    int unwrappedphase_intpart;
    int priorStep = -1;
    float ratemult;
    float env_releasestart;
    float iout;
    float wf_history[4];
    bool is_display;
    int step, shuffle_id;
    int magn, rate, iattack, idecay, idelay, ihold, isustain, irelease, startphase, ideform;

    float onepoleState[3];

    std::default_random_engine gen;
    std::uniform_real_distribution<float> distro;
    std::function<float()> urng;
    using quadr_osc = sst::basic_blocks::dsp::SurgeQuadrOsc<float>;
    quadr_osc sinus;
};

#endif // SURGE_SRC_COMMON_DSP_MODULATORS_LFOMODULATIONSOURCE_H
