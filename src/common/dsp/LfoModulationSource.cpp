#include "LfoModulationSource.h"
#include <cmath>
#include "DebugHelpers.h"

using namespace std;

LfoModulationSource::LfoModulationSource()
{}

void LfoModulationSource::assign(SurgeStorage* storage,
                                 LFOStorage* lfo,
                                 pdata* localcopy,
                                 SurgeVoiceState* state,
                                 StepSequencerStorage* ss,
                                 bool is_display)
{
   this->storage = storage;
   this->lfo = lfo;
   this->state = state;
   this->localcopy = localcopy;
   this->ss = ss;
   this->is_display = is_display;

   iout = 0;
   output = 0;
   step = 0;
   env_state = lenv_delay;
   env_val = 0.f;
   env_phase = 0;
   shuffle_id = 0;
   ratemult = 1.f;
   retrigger_FEG = false;
   retrigger_AEG = false;
   priorPhase = -1000;
   
   rate = lfo->rate.param_id_in_scene;
   magn = lfo->magnitude.param_id_in_scene;
   idelay = lfo->delay.param_id_in_scene;
   iattack = lfo->attack.param_id_in_scene;
   idecay = lfo->decay.param_id_in_scene;
   ihold = lfo->hold.param_id_in_scene;
   isustain = lfo->sustain.param_id_in_scene;
   irelease = lfo->release.param_id_in_scene;
   startphase = lfo->start_phase.param_id_in_scene;
   ideform = lfo->deform.param_id_in_scene;

   phaseInitialized = false;

   if (is_display)
      srand(17);
   noise = 0.f;
   noised1 = 0.f;
   target = 0.f;
   for (int i = 0; i < 4; i++)
      wf_history[i] = 0.f; //((float) rand()/(float)RAND_MAX)*2.f - 1.f;
}

float LfoModulationSource::bend1(float x)
{
   float a = localcopy[ideform].f;

   x += 0.25;
   x += a * sin(x * 2.f * M_PI) / (2.f * M_PI);
   x -= 0.25;

   return x;
}

float LfoModulationSource::bend2(float x)
{
   float a = localcopy[ideform].f;

   x += a * sin(x * 2.f * M_PI) / (2.f * M_PI);

   return x;
}

float LfoModulationSource::bend3(float x)
{
   float a = 0.5f * limit_range( localcopy[ideform].f, -3.f, 3.f );

   x = x - a * x * x + a;
   x = x - a * x * x + a; // do twice for extra pleasure

   return x;
}

float CubicInterpolate(float y0, float y1, float y2, float y3, float mu)
{
   float a0, a1, a2, a3, mu2;

   mu2 = mu * mu;
   a0 = y3 - y2 - y0 + y1;
   a1 = y0 - y1 - a0;
   a2 = y2 - y0;
   a3 = y1;

   return (a0 * mu * mu2 + a1 * mu2 + a2 * mu + a3);
}

void LfoModulationSource::initPhaseFromStartPhase()
{
   phase = localcopy[startphase].f;
   phaseInitialized = true;
   while( phase < 0.f )
      phase += 1.f;
   while( phase > 1.f )
      phase -= 1.f;
}

void LfoModulationSource::attack()
{
   if( ! phaseInitialized )
   {
      initPhaseFromStartPhase();
   }

   env_state = lenv_delay;

   env_val = 0.f;
   env_phase = 0;
   ratemult = 1.f;

   if (localcopy[idelay].f == lfo->delay.val_min.f)
   {
      env_state = lenv_attack;
      if (localcopy[iattack].f == lfo->attack.val_min.f)
      {
         env_state = lenv_hold;
         env_val = 1.f;
         if (localcopy[ihold].f == lfo->hold.val_min.f)
            env_state = lenv_decay;
      }
   }

   if (is_display)
   {
      phase = lfo->start_phase.val.f;
      if (lfo->shape.val.i == ls_stepseq)
         phase = 0.f;
      step = 0;
   }
   else
   {
      float phaseslider;
      if (lfo->shape.val.i == ls_stepseq)
         phaseslider = 0.f; // Use Phase as shuffle-parameter instead
      // else if(state) phaseslider = lfo->start_phase.val.f;
      else
         phaseslider = localcopy[startphase].f;

      // With modulation the phaseslider can be outside [0,1], as in #1524
      while( phaseslider < 0.f )
         phaseslider += 1.f;
      while( phaseslider > 1.f )
         phaseslider -= 1.f;

      switch (lfo->trigmode.val.i)
      {
      case lm_keytrigger:
         phase = phaseslider;
         step = 0;
         break;
      case lm_random:
         phase = (float)rand() / (float)RAND_MAX;
         if( ss->loop_end == 0 )
            step = 0;
         else
            step = (rand() % ss->loop_end) & (n_stepseqsteps - 1);
         break;
      case lm_freerun:
      {
         double ipart; //,tsrate = localcopy[rate].f;
         phase = (float)modf(
             phaseslider + 0.5f * storage->songpos * pow(2.0, (double)localcopy[rate].f), &ipart);
         int i = (int)ipart;
         step = (i % max(1, (ss->loop_end - ss->loop_start))) + ss->loop_start;
      }
      break;
      default:
         step = 0;
         phase = 0;
         break;
      };
   }

   switch (lfo->shape.val.i)
   {
   case ls_snh:
      noise = 0.f;
      noised1 = 0.f;
      target = 0.f;
      iout = correlated_noise_o2mk2(target, noised1, limit_range(localcopy[ideform].f,-1.f,1.f));
      break;
   case ls_stepseq:
   {
      // fire up the engines
      wf_history[1] = ss->steps[step & (n_stepseqsteps - 1)];
      wf_history[2] = ss->steps[(step+n_stepseqsteps-1) & (n_stepseqsteps - 1)];
      wf_history[3] = ss->steps[(step+n_stepseqsteps-2) & (n_stepseqsteps - 1)];

      step++;

      if (ss->loop_end > ss->loop_start)
      {
          if (step > ss->loop_end)
             step = ss->loop_start;
      }
      else
      {
          if (step >= ss->loop_start)
             step = ss->loop_end + 1;
      }
      shuffle_id = (shuffle_id + 1) & 1;
      if (shuffle_id)
         ratemult = 1.f / max(0.01f, 1.f - 0.5f * lfo->start_phase.val.f);
      else
         ratemult = 1.f / (1.f + 0.5f * lfo->start_phase.val.f);

      wf_history[0] = ss->steps[step & (n_stepseqsteps - 1)];
   }
   break;
   case ls_noise:
   {
      noise = 0.f;
      noised1 = 0.f;
      target = 0.f;
      auto lid = limit_range(localcopy[ideform].f,-1.f,1.f);
      wf_history[3] = correlated_noise_o2mk2(target, noised1, lid) * phase;
      wf_history[2] = correlated_noise_o2mk2(target, noised1, lid) * phase;
      wf_history[1] = correlated_noise_o2mk2(target, noised1, lid) * phase;
      wf_history[0] = correlated_noise_o2mk2(target, noised1, lid) * phase;
      /*wf_history[0] = 0.f;
      wf_history[1] = 0.f;
      wf_history[2] = 0.f;
      wf_history[3] = 0.f;*/
      phase = 0.f;
   }
   break;
   case ls_tri:
   {
      if (!lfo->unipolar.val.b)
      {
         phase += 0.25;
         if (phase > 1.f)
            phase -= 1.f;
      }
   }
   break;
   case ls_sine:
   {
      if (lfo->unipolar.val.b)
      {
         phase += 0.75;
         if (phase > 1.f)
            phase -= 1.f;
      }
   }
   break;
   }
}

void LfoModulationSource::release()
{
   if (lfo->release.val.f < lfo->release.val_max.f)
   {
      env_state = lenv_release;
      env_releasestart = env_val;
      env_phase = 0;
   }
}

void LfoModulationSource::process_block()
{
   if( (! phaseInitialized) || ( lfo->trigmode.val.i == lm_keytrigger && lfo->rate.deactivated ) )
   {
      initPhaseFromStartPhase();
   }

   retrigger_FEG = false;
   retrigger_AEG = false;
   int s = lfo->shape.val.i;

   float frate = envelope_rate_linear_nowrap(-localcopy[rate].f);
   
   if (lfo->rate.deactivated)
      frate = 0.0;

   if (lfo->rate.temposync)
      frate *= storage->temposyncratio;
   phase += frate * ratemult;

   if( frate == 0 && phase == 0 && s == ls_stepseq )
   {
      phase = 0.001; // step forward a smidge
   }
   
   /*if (lfo->trigmode.val.i == lm_freerun)
   {
   double ipart; //,tsrate = localcopy[rate].f;
   phase = (float)modf(0.5f*storage->songpos*pow(2.0,(double)localcopy[rate].f),&ipart);
   //phase = storage->songpos?
   }*/

   if (env_state != lenv_stuck)
   {
      float envrate = 0;

      switch (env_state)
      {
      case lenv_delay:
         envrate = envelope_rate_linear_nowrap(localcopy[idelay].f);
         if (lfo->delay.temposync)
            envrate *= storage->temposyncratio;
         break;
      case lenv_attack:
         envrate = envelope_rate_linear_nowrap(localcopy[iattack].f);
         if (lfo->attack.temposync)
            envrate *= storage->temposyncratio;
         break;
      case lenv_hold:
         envrate = envelope_rate_linear_nowrap(localcopy[ihold].f);
         if (lfo->hold.temposync)
            envrate *= storage->temposyncratio;
         break;
      case lenv_decay:
         envrate = envelope_rate_linear_nowrap(localcopy[idecay].f);
         if (lfo->decay.temposync)
            envrate *= storage->temposyncratio;
         break;
      case lenv_release:
         envrate = envelope_rate_linear_nowrap(localcopy[irelease].f);
         if (lfo->release.temposync)
            envrate *= storage->temposyncratio;
         break;
      };
      env_phase += envrate;

      float sustainlevel = localcopy[isustain].f;
      // sustainlevel = sustainlevel*(1.f + localcopy[ideform].f) -
      // sustainlevel*sustainlevel*localcopy[ideform].f; sustainlevel = sustainlevel /
      // (sustainlevel*localcopy[ideform].f + 1 - localcopy[ideform].f); float dd =
      // (localcopy[ideform].f - 1); if (s == ls_constant1) sustainlevel = 0.5f *
      // (sqrt(4.f*sustainlevel + dd*dd) + dd); if (s == ls_constant1) sustainlevel = 1.f / (1.f +
      // localcopy[ideform].f); a = (1.f-localcopy[ideform].f) + localcopy[ideform].f*env_val;
      // u = e*a;

      if (env_phase > 1.f)
      {
         switch (env_state)
         {
         case lenv_delay:
            env_state = lenv_attack;
            env_phase = 0.f; // min(1.f, env_phase-1.f);
            break;
         case lenv_attack:
            env_state = lenv_hold;
            env_phase = 0.f; // min(1.f, env_phase-1.f);
            break;
         case lenv_hold:
            env_state = lenv_decay;
            env_phase = 0.f; // min(1.f, env_phase-1.f);
            /*if (localcopy[idecay].f == lfo->decay.val_max.f)
            {
            env_state = lenv_stuck;
            env_val = 1.f;
            env_phase = 0;
            }*/
            break;
         case lenv_decay:
            env_state = lenv_stuck;
            env_phase = 0;
            env_val = sustainlevel;
            break;
         case lenv_release:
            env_state = lenv_stuck;
            env_phase = 0;
            env_val = 0.f;
            break;
         };
      }
      switch (env_state)
      {
      case lenv_delay:
         env_val = 0.f;
         break;
      case lenv_attack:
         env_val = env_phase;
         break;
      case lenv_hold:
         env_val = 1.f;
         break;
      case lenv_decay:
         env_val = (1.f - env_phase) + env_phase * sustainlevel;
         break;
      case lenv_release:
         env_val = (1.f - env_phase) * env_releasestart;
         break;
      };
   }

   if (phase > 1 || phase < 0 )
   {
      if( phase >= 2 )
      {
         float ipart;
         phase = modf( phase, &ipart );
      }
      else if( phase < 0 )
      {
         // -6.02 needs to go to .98
         //
         int p = (int) phase - 1;
         float np = -p + phase;
         if( np >= 0 && np < 1 )
            phase = np;
         else
            phase = 0; // should never get here but something is already wierd with the mod stack
      }
      else
      {
         phase -= 1;
      }

      switch (s)
      {
      case ls_snh:
      {
         iout = correlated_noise_o2mk2(target, noised1, limit_range(localcopy[ideform].f,-1.f,1.f));
      }
      break;
      case ls_noise:
      {
         wf_history[3] = wf_history[2];
         wf_history[2] = wf_history[1];
         wf_history[1] = wf_history[0];

         wf_history[0] = correlated_noise_o2mk2(target, noised1, limit_range( localcopy[ideform].f, -1.f, 1.f ) ); 
         // target = ((float) rand()/RAND_MAX)*2.f - 1.f;
      }
      break;
      case ls_stepseq:
         /*
         ** You might thing we don't need this and technically we don't
         ** but I wanted to keep it here to retain compatability with 
         ** versions of trigmask which were streamed in older sessions
         */
         if (ss->trigmask & (UINT64_C(1) << step))
         {
            retrigger_FEG = true;
            retrigger_AEG = true;
         }
         if (ss->trigmask & (UINT64_C(1) << (16+step)))
         {
            retrigger_FEG = true;
         }
         if (ss->trigmask & (UINT64_C(1) << (32+step)))
         {
            retrigger_AEG = true;
         }
         step++;
         shuffle_id = (shuffle_id + 1) & 1;
         if (shuffle_id)
            ratemult = 1.f / max(0.01f, 1.f - 0.5f * lfo->start_phase.val.f);
         else
            ratemult = 1.f / (1.f + 0.5f * lfo->start_phase.val.f);

         if (ss->loop_end > ss->loop_start)
         {
            if (step > ss->loop_end)
               step = ss->loop_start;
         }
         else
         {
            if (step >= ss->loop_start)
               step = ss->loop_end + 1;
         }
         wf_history[3] = wf_history[2];
         wf_history[2] = wf_history[1];
         wf_history[1] = wf_history[0];
         wf_history[0] = ss->steps[step & (n_stepseqsteps - 1)];
         break;
      };
   }

   switch (s)
   {
   case ls_constant1:
      iout = (1.f - localcopy[ideform].f) + localcopy[ideform].f * env_val;
      break;
   case ls_sine:
      iout = bend3(lookup_waveshape_warp(3, 2.f - 4.f * phase));
      break;
   case ls_tri:
      iout = bend3(-1.f + 4.f * ((phase > 0.5) ? (1 - phase) : phase));
      break;
   case ls_ramp:
      iout = bend3(1.f - 2.f * phase);
      break;
   case ls_square:
      iout = (phase > (0.5f + 0.5f * localcopy[ideform].f)) ? -1.f : 1.f;
      break;
   case ls_noise:
   {
      // iout = noise*(1-phase) + phase*target;
      iout = CubicInterpolate(wf_history[3], wf_history[2], wf_history[1], wf_history[0], phase);
   }
   break;
   case ls_stepseq:
      // iout = wf_history[0];
      {
         // Support 0 rate scrubbing across all 16 steps
         float calcPhase = phase;
         if( frate == 0 )
         {
            /*
            ** Alright so in 0 rate mode we want to scrub through tne entire sequence. So
            */
            float p16 = phase * n_stepseqsteps;
            int pstep = ( (int)p16 ) & ( n_stepseqsteps - 1 );
            float sphase = ( p16 - pstep );

            // OK so pstep is now between 0 and ns-1. But if that is beyond the end we want to wrap
            int last_step = std::max( ss->loop_end, ss->loop_start );
            int loop_len = std::max( 1, abs( ss->loop_end - ss->loop_start ) + 1 );
            while( pstep > last_step && pstep >= 0 ) pstep -= loop_len;
            pstep = pstep & ( n_stepseqsteps - 1 );

            if( priorPhase != phase )
            {
               priorPhase = phase;

               /*
               ** So we want to load up the wf_history
               */
               for( int i=0; i<4; ++i )
               {
                  wf_history[i] = ss->steps[( pstep + 1 + n_stepseqsteps - i ) & (n_stepseqsteps - 1)];
               }
            }
            
            calcPhase = sphase;
         }
         float df = localcopy[ideform].f;
         if (df > 0.5f)
         {
            float linear = (1.f - calcPhase) * wf_history[2] + calcPhase * wf_history[1];
            float cubic =
                CubicInterpolate(wf_history[3], wf_history[2], wf_history[1], wf_history[0], calcPhase);
            iout = (2.f - 2.f * df) * linear + (2.f * df - 1.0f) * cubic;
         }
         else if (df > -0.0001f)
         {
            float cf = max(0.f, min(calcPhase / (2.f * df + 0.00001f), 1.0f));
            iout = (1.f - cf) * wf_history[2] + cf * wf_history[1];
         }
         else if (df > -0.5f)
         {
            float cf = max(0.f, min((1.f - calcPhase) / (-2.f * df + 0.00001f), 1.0f));
            iout = (1.f - cf) * 0 + cf * wf_history[1];
         }
         else
         {
            float cf = max(0.f, min(calcPhase / (2.f + 2.f * df + 0.00001f), 1.0f));
            iout = (1.f - cf) * wf_history[1] + cf * 0;
         }
      }
      break;
   };

   float io2 = iout;

   if (lfo->unipolar.val.b)
   {
      if (s != ls_stepseq)
      {
         io2 = 0.5f + 0.5f * io2;
      }
      else if (io2 < 0.f)
      {
         io2 = 0.f;
      }
   }

   auto magnf = limit_range( localcopy[magn].f, -3.f, 3.f );
   output = env_val * magnf * io2;
}
