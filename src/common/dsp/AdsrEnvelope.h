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
#include "DebugHelpers.h"

enum AdsrState
{
   s_attack = 0,
   s_decay,
   s_sustain,
   s_release,
   s_uberrelease,
   s_idle_wait1,
   s_idle,
};

const float one = 1.f;
const float zero = 0.f;
const float db96 = powf(10.f, 0.05f * -96.f);
const float db60 = powf(10.f, 0.05f * -60.f);

class AdsrEnvelope : public ModulationSource
{
public:
   AdsrEnvelope()
   {
   }

   void init(SurgeStorage* storage, ADSRStorage* adsr, pdata* localcopy, SurgeVoiceState* state)
   {
      this->storage = storage;
      this->adsr = adsr;
      this->state = state;
      this->lc = localcopy;

      a = adsr->a.param_id_in_scene;
      d = adsr->d.param_id_in_scene;
      s = adsr->s.param_id_in_scene;
      r = adsr->r.param_id_in_scene;

      a_s = adsr->a_s.param_id_in_scene;
      d_s = adsr->d_s.param_id_in_scene;
      r_s = adsr->r_s.param_id_in_scene;

      mode = adsr->mode.param_id_in_scene;

      envstate = s_attack;

      phase = 0;
      output = 0;
      idlecount = 0;
      scalestage = 1.f;
      _v_c1 = 0.f;
      _v_c1_delayed = 0.f;
      _discharge = 0.f;
   }

   void retrigger()
   {
      if (envstate < s_release)
         attack();
   }

   virtual void attack() override
   {
      phase = 0;
      output = 0;
      idlecount = 0;
      scalestage = 1.f;

      // Reset the analog state machine too
      _v_c1 = 0.f;
      _v_c1_delayed = 0.f;
      _discharge = 0.f;

      envstate = s_attack;
      if ((lc[a].f - adsr->a.val_min.f) < 0.01)
      {
         envstate = s_decay;
         output = 1;
         phase = 1;
      }
   }

   virtual const char* get_title() override
   {
      return "envelope";
   }
   virtual int get_type() override
   {
      return mst_adsr;
   }
   virtual bool per_voice() override
   {
      return true;
   }
   virtual bool is_bipolar() override
   {
      return false;
   }

   void release() override
   {
      /*if(envstate == s_attack)
      {
              phase = powf(phase,(float)(1.0f+lc[a_s].i)/(1.0f+lc[r_s].i));
      }
      else
      if(envstate == s_decay)
      {
              phase = powf(phase,(float)1.0f/(1.0f+lc[r_s].i));
      }
      phase = limit_range(phase,0,1);*/
      scalestage = output;
      phase = 1;
      envstate = s_release;
   }
   void uber_release()
   {
      /*if(envstate == s_attack)
      {
              phase = powf(phase,(float)(1.0f+lc[a_s].i)/(1.0f+lc[r_s].i));
      }
      else
      if(envstate == s_decay)
      {
              phase = powf(phase,(float)1.0f/(1.0f+lc[r_s].i));
      }
      phase = limit_range(phase,0,1);*/
      scalestage = output;
      phase = 1;
      envstate = s_uberrelease;
   }
   bool is_idle()
   {
      return (envstate == s_idle) && (idlecount > 0);
   }
   virtual void process_block() override
   {
      if (lc[mode].b)
      {
         /*
         ** This is the "analog" mode of the envelope. If you are unclear what it is doing
         ** because of the SSE the algo is pretty simple; charge up and discharge a capacitor
         ** with a gate. charge until you hit 1, discharge while the gate is open floored at
         ** the Sustain; then release.
         **
         ** There is, in src/headless/UnitTests.cpp in the "Clone the Analog" section, 
         ** a non-SSE implementation of this which makes it much easier to understand.
         */
         const float v_cc = 1.5f;

         __m128 v_c1 = _mm_load_ss(&_v_c1);
         __m128 v_c1_delayed = _mm_load_ss(&_v_c1_delayed);
         __m128 discharge = _mm_load_ss(&_discharge);
         const __m128 one = _mm_set_ss(1.0f); // attack->decay switch at 1 volt
         const __m128 v_cc_vec = _mm_set_ss(v_cc);

         bool gate = (envstate == s_attack) || (envstate == s_decay);
         __m128 v_gate = gate ? _mm_set_ss(v_cc) : _mm_set_ss(0.f);
         __m128 v_is_gate = _mm_cmpgt_ss( v_gate, _mm_set_ss( 0.0 ) );

         // The original code here was
         // _mm_and_ps(_mm_or_ps(_mm_cmpgt_ss(v_c1_delayed, one), discharge), v_gate);
         // which ored in the v_gate value as opposed to the boolean
         discharge = _mm_and_ps( _mm_or_ps(_mm_cmpgt_ss(v_c1_delayed, one), discharge), v_is_gate );
                                 
         v_c1_delayed = v_c1;

         float sparm = limit_range( lc[s].f, 0.f, 1.f );
         __m128 S = _mm_load_ss(&sparm);
         S = _mm_mul_ss(S, S);
         __m128 v_attack = _mm_andnot_ps(discharge, v_gate);
         __m128 v_decay = _mm_or_ps(_mm_andnot_ps(discharge, v_cc_vec), _mm_and_ps(discharge, S));
         __m128 v_release = v_gate;

         __m128 diff_v_a = _mm_max_ss(_mm_setzero_ps(), _mm_sub_ss(v_attack, v_c1));

         // This change from a straight min allows sustain swells
         __m128 diff_vd_kernel = _mm_sub_ss(v_decay, v_c1);
         __m128 diff_vd_kernel_min = _mm_min_ss(_mm_setzero_ps(), diff_vd_kernel );
         __m128 dis_and_gate = _mm_and_ps(discharge, v_is_gate );
         __m128 diff_v_d = _mm_or_ps(_mm_and_ps(dis_and_gate, diff_vd_kernel),
                                     _mm_andnot_ps(dis_and_gate, diff_vd_kernel_min));
                                                
         __m128 diff_v_r = _mm_min_ss(_mm_setzero_ps(), _mm_sub_ss(v_release, v_c1));

         // calculate coefficients for envelope
         const float shortest = 6.f;
         const float longest = -2.f;
         const float coeff_offset = 2.f - log(samplerate / BLOCK_SIZE) / log(2.f);

         float coef_A =
             powf(2.f, std::min(0.f, coeff_offset -
                                    lc[a].f * (adsr->a.temposync ? storage->temposyncratio : 1.f)));
         float coef_D =
             powf(2.f, std::min(0.f, coeff_offset -
                                    lc[d].f * (adsr->d.temposync ? storage->temposyncratio : 1.f)));
         float coef_R =
             envstate == s_uberrelease
                 ? 6.f
                 : powf(2.f, std::min(0.f, coeff_offset -
                                          lc[r].f *
                                              (adsr->r.temposync ? storage->temposyncratio : 1.f)));

         v_c1 = _mm_add_ss(v_c1, _mm_mul_ss(diff_v_a, _mm_load_ss(&coef_A)));
         v_c1 = _mm_add_ss(v_c1, _mm_mul_ss(diff_v_d, _mm_load_ss(&coef_D)));
         v_c1 = _mm_add_ss(v_c1, _mm_mul_ss(diff_v_r, _mm_load_ss(&coef_R)));

         _mm_store_ss(&_v_c1, v_c1);
         _mm_store_ss(&_v_c1_delayed, v_c1_delayed);
         _mm_store_ss(&_discharge, discharge);

         _mm_store_ss(&output, v_c1);

         const float SILENCE_THRESHOLD = 1e-6;

         if (!gate && _discharge == 0.f && _v_c1 < SILENCE_THRESHOLD)
         {
            envstate = s_idle;
            output = 0;
            idlecount++;
         }
      }
      else
      {
         switch (envstate)
         {
         case (s_attack):
         {
            phase +=
                envelope_rate_linear_nowrap(lc[a].f) * (adsr->a.temposync ? storage->temposyncratio : 1.f);
            if (phase >= 1)
            {
               phase = 1;
               envstate = s_decay;
               sustain = lc[s].f;
            }
            /*output = phase;
            for(int i=0; i<lc[a_s].i; i++) output *= phase;*/
            switch (lc[a_s].i)
            {
            case 0:
               output = sqrt(phase);
               break;
            case 1:
               output = phase;
               break;
            case 2:
               output = phase * phase;
               break;
            };
         }
         break;
         case (s_decay):
         {
            /*phase -= (1-sustain) * envelope_rate_linear(adsr->d.val.f);
            if(phase < sustain)
            {
            phase = sustain;
            }*/
            float rate =
               envelope_rate_linear_nowrap(lc[d].f) * (adsr->d.temposync ? storage->temposyncratio : 1.f);

            float l_lo, l_hi;

            switch (lc[d_s].i)
            {
            case 1:
            {
               float sx = sqrt(phase);
               l_lo = phase - 2 * sx * rate + rate * rate;
               l_hi = phase + 2 * sx * rate + rate * rate;

               /*
               ** That + rate * rate in both means at low sustain ( < 1e-3 or so) you end up with
               ** lo and hi both pushing us up off sustain. Unfortunatley we ned to handle that case
               ** specially by pushing lo down. These limits are pretty empirical. Git blame to see
               ** the various issues around here which show the test cases.
               */
               if( ( lc[s].f < 1e-3 && phase < 1e-4 ) || ( lc[s].f == 0 && lc[d].f < -7 ) )
                  l_lo = 0;
               /*
               ** Similarly if the rate is very high - larger than one - we can push l_lo well above the
               ** sustain, which can set back a feedback cycle where we bounce onto sustain and off again.
               ** To understand this, remove this bound and play with test-data/patches/ADSR-Self-Oscillate.fxp
               */
               if( rate > 1.0 && l_lo > lc[s].f )
                  l_lo = lc[s].f;
            }
            break;
            case 2:
            {
               float sx = powf(phase, 0.3333333f);
               l_lo = phase - 3 * sx * sx * rate + 3 * sx * rate * rate - rate * rate * rate;
               l_hi = phase + 3 * sx * sx * rate + 3 * sx * rate * rate + rate * rate * rate;
            }
            break;
            default:
               l_lo = phase - rate;
               l_hi = phase + rate;
               break;
            };

            phase = limit_range(lc[s].f, l_lo, l_hi);
            output = phase;


         }
         break;
         case (s_release):
         {
            phase -=
                envelope_rate_linear_nowrap(lc[r].f) * (adsr->r.temposync ? storage->temposyncratio : 1.f);
            output = phase;
            for (int i = 0; i < lc[r_s].i; i++)
               output *= phase;

            if (phase < 0)
            {
               envstate = s_idle;
               output = 0;
            }
            output *= scalestage;
         }
         break;
         case (s_uberrelease):
         {
            phase -= envelope_rate_linear_nowrap(-6.5);
            output = phase;
            for (int i = 0; i < lc[r_s].i; i++)
               output *= phase;
            if (phase < 0)
            {
               envstate = s_idle;
               output = 0;
            }
            output *= scalestage;
         }
         break;
         case s_idle:
            idlecount++;
            break;
         };

         output = limit_range(output, 0.f, 1.f);
      }
   }

   int getEnvState() { return envstate; }
   
private:
   ADSRStorage* adsr = nullptr;
   SurgeVoiceState* state = nullptr;
   SurgeStorage* storage = nullptr;
   float phase = 0.f;
   float sustain = 0.f;
   float scalestage = 0.f;
   int idlecount = 0;
   int envstate = 0;
   pdata* lc = nullptr;
   int a = 0, d = 0, s = 0, r = 0, a_s = 0, d_s = 0, r_s = 0, mode = 0;

   float _v_c1 = 0.f;
   float _v_c1_delayed = 0.f;
   float _discharge = 0.f;
};
