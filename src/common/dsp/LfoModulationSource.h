//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "DspUtilities.h"
#include "SurgeStorage.h"
#include "SurgeVoiceState.h"
#include "ModulationSource.h"

enum lfoenv_state
{
   lenv_off = 0,
   lenv_delay,
   lenv_attack,
   lenv_hold,
   lenv_decay,
   lenv_release,
   lenv_stuck,
};

class LfoModulationSource : public ModulationSource
{
public:
   LfoModulationSource();
   void assign(SurgeStorage* storage,
               LFOStorage* lfo,
               pdata* localcopy,
               SurgeVoiceState* state,
               StepSequencerStorage* ss,
               bool is_display = false);
   float bend1(float x);
   float bend2(float x);
   float bend3(float x);
   virtual void attack() override;
   virtual void release() override;
   virtual void process_block() override;

   virtual const char* get_title() override
   {
      return "LFO";
   }
   virtual int get_type() override
   {
      return mst_lfo;
   }
   virtual bool is_bipolar() override
   {
      return true;
   }
   float env_val;
   int env_state;
   bool retrigger_FEG;
   bool retrigger_AEG;

private:
   LFOStorage* lfo;
   SurgeVoiceState* state;
   SurgeStorage* storage;
   StepSequencerStorage* ss;
   pdata* localcopy;
   float phase, target, noise, noised1, env_phase;
   float ratemult;
   float env_releasestart;
   float iout;
   float wf_history[4];
   bool is_display;
   int step, shuffle_id;
   int magn, rate, iattack, idecay, idelay, ihold, isustain, irelease, startphase, ideform;
   quadr_osc sinus;
};
