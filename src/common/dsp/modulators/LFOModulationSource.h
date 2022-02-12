/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#pragma once

#include "DSPUtils.h"
#include "SurgeStorage.h"
#include "SurgeVoiceState.h"
#include "ModulationSource.h"
#include "MSEGModulationHelper.h" // We need this for the MSEGEvalatorState member
#include "FormulaModulationHelper.h"
#include <functional>

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
    virtual void attack() override;
    virtual void release() override;
    virtual void process_block() override;
    virtual void completedModulation();

    int get_active_outputs() override
    {
        if (lfo->shape.val.i == lt_formula)
            return Surge::Formula::max_formula_outputs;
        else
            return 3;
    }
    float get_output(int which) override { return output_multi[which]; }
    float get_output01(int which) override { return output_multi[which]; }
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

  private:
    LFOStorage *lfo;
    SurgeVoiceState *state;
    SurgeStorage *storage;
    StepSequencerStorage *ss;
    MSEGStorage *ms;

    FormulaModulatorStorage *fs;

    float output_multi[Surge::Formula::max_formula_outputs];

  public:
    Surge::MSEG::EvaluatorState msegstate;
    Surge::Formula::EvaluatorState formulastate;

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

    std::default_random_engine gen;
    std::uniform_real_distribution<float> distro;
    std::function<float()> urng;
    static int urngSeed;
    quadr_osc sinus;
};
