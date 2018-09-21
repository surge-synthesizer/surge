//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "dsputils.h"
#include "storage.h"
#include "voice_state.h"
#include "modulation.h"

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

class modulation_lfo : public modulation_source
{
public:
   modulation_lfo();
   void assign(sub3_storage* storage,
               sub3_lfo* lfo,
               pdata* localcopy,
               sub3_voice_state* state,
               sub3_stepsequence* ss,
               bool is_display = false);
   float bend1(float x);
   float bend2(float x);
   float bend3(float x);
   virtual void attack();
   virtual void release();
   virtual void process_block();

   virtual const char* get_title()
   {
      return "LFO";
   }
   virtual int get_type()
   {
      return mst_lfo;
   }
   virtual bool is_bipolar()
   {
      return true;
   }
   float env_val;
   bool retrigger_EG;

private:
   sub3_lfo* lfo;
   sub3_voice_state* state;
   sub3_storage* storage;
   sub3_stepsequence* ss;
   pdata* localcopy;
   float phase, target, noise, noised1, env_phase;
   float ratemult;
   float env_releasestart;
   float iout;
   float wf_history[4];
   bool is_display;
   int env_state, step, shuffle_id;
   int magn, rate, iattack, idecay, idelay, ihold, isustain, irelease, startphase, ideform;
   quadr_osc sinus;
};