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

#include "DspUtilities.h"
#include "SurgeStorage.h"
#include "SurgeVoiceState.h"
#include "ModulationSource.h"
#include "MSEGModulationHelper.h" // We need this for the MSEGEvalatorState member
#include <functional>

enum lfoenv_state
{
    lenv_off = 0,
    lenv_delay,
    lenv_attack,
    lenv_hold,
    lenv_decay,
    lenv_release,
    lenv_msegrelease,
    lenv_stuck,
};

class LfoModulationSource : public ModulationSource
{
  public:
    LfoModulationSource();
    void assign(SurgeStorage *storage, LFOStorage *lfo, pdata *localcopy, SurgeVoiceState *state,
                StepSequencerStorage *ss, MSEGStorage *ms, FormulaModulatorStorage *fs,
                bool is_display = false);
    float bend1(float x);
    float bend2(float x);
    float bend3(float x);
    virtual void attack() override;
    virtual void release() override;
    virtual void process_block() override;

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
    Surge::MSEG::EvaluatorState msegstate;
    FormulaModulatorStorage *fs;
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
