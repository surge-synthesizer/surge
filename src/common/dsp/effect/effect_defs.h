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

#include "Effect.h"
#include "BiquadFilter.h"
#include "DspUtilities.h"
#include "AllpassFilter.h"

#include "VectorizedSvfFilter.h"

#include <vt_dsp/halfratefilter.h>
#include <vt_dsp/lipol.h>

const int max_delay_length = 1 << 18;

const int slowrate = 8;
const int slowrate_m1 = slowrate - 1;

class DualDelayEffect : public Effect
{
   lipol_ps feedback alignas(16),
            crossfeed alignas(16),
            aligpan alignas(16),
            pan alignas(16),
            mix alignas(16),
            width alignas(16);
   float buffer alignas(16)[2][max_delay_length + FIRipol_N];

public:
   DualDelayEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~DualDelayEffect();
   virtual const char* get_effectname() override
   {
      return "dualdelay";
   }
   virtual void init() override;
   virtual void process(float* dataL, float* dataR) override;
   virtual void suspend() override;
   void setvars(bool init);
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;
   virtual const char* group_label(int id) override;
   virtual int group_label_ypos(int id) override;
   virtual int get_ringout_decay() override
   {
      return ringout_time;
   }

private:
   lag<float, true> timeL, timeR;
   bool inithadtempo;
   float envf;
   int wpos;
   BiquadFilter lp, hp;
   double lfophase;
   float LFOval;
   bool LFOdirection;
   int ringout_time;
};

template <int v> class ChorusEffect : public Effect
{
   lipol_ps feedback alignas(16),
            mix alignas(16),
            width alignas(16);
   __m128 voicepanL4 alignas(16)[v],
          voicepanR4 alignas(16)[v];
   float buffer alignas(16)[max_delay_length + FIRipol_N]; // Includes padding so we can use SSE
                                                           // interpolation without wrapping
public:
   ChorusEffect<v>(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~ChorusEffect();
   virtual const char* get_effectname() override
   {
      return "chorus";
   }
   virtual void init() override;
   virtual void process(float* dataL, float* dataR) override;
   virtual void suspend() override;
   void setvars(bool init);
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;
   virtual const char* group_label(int id) override;
   virtual int group_label_ypos(int id) override;

private:
   lag<float, true> time[v];
   float voicepan[v][2];
   float envf;
   int wpos;
   BiquadFilter lp, hp;
   double lfophase[v];
};

class FreqshiftEffect : public Effect
{
public:
   HalfRateFilter fr alignas(16),
                  fi alignas(16);
   lipol_ps mix alignas(16);
   FreqshiftEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~FreqshiftEffect();
   virtual const char* get_effectname() override
   {
      return "freqshift";
   }
   virtual void init() override;
   virtual void process(float* dataL, float* dataR) override;
   virtual void suspend() override;
   void setvars(bool init);
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;
   virtual const char* group_label(int id) override;
   virtual int group_label_ypos(int id) override;
   virtual int get_ringout_decay() override
   {
      return ringout_time;
   }

private:
   lipol<float, true> feedback;
   lag<float, true> time, shiftL, shiftR;
   bool inithadtempo;
   float buffer[2][max_delay_length];
   int wpos;
   // CHalfBandFilter<6> frL,fiL,frR,fiR;
   quadr_osc o1L, o2L, o1R, o2R;
   int ringout_time;
};

class Eq3BandEffect : public Effect
{
   lipol_ps gain alignas(16);
   lipol_ps mix alignas(16);

   float L alignas(16)[BLOCK_SIZE],
         R alignas(16)[BLOCK_SIZE];

public:
   enum Params
   {
       eq3_gain1 = 0,
       eq3_freq1,
       eq3_bw1,

       eq3_gain2,
       eq3_freq2,
       eq3_bw2,

       eq3_gain3,
       eq3_freq3,
       eq3_bw3,

       eq3_gain,
       eq3_mix,

       eq3_num_ctrls,
   };

   Eq3BandEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~Eq3BandEffect();
   virtual const char* get_effectname() override
   {
      return "EQ";
   }
   virtual void init() override;
   virtual void process(float* dataL, float* dataR) override;
   virtual int get_ringout_decay() override
   {
      return 500;
   }
   virtual void suspend() override;
   void setvars(bool init);
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;
   virtual const char* group_label(int id) override; 
   virtual int group_label_ypos(int id) override;

   virtual void handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision) override;


private:
   BiquadFilter band1, band2, band3;
   int bi; // block increment (to keep track of events not occurring every n blocks)
};

class PhaserEffect : public Effect
{
   lipol_ps mix alignas(16);
   float L alignas(16)[BLOCK_SIZE],
         R alignas(16)[BLOCK_SIZE];

public:
   PhaserEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~PhaserEffect();
   virtual const char* get_effectname() override
   {
      return "phaser";
   }
   virtual void init() override;
   virtual void process_only_control() override;
   virtual void process(float* dataL, float* dataR) override;
   virtual int get_ringout_decay() override
   {
      return 1000;
   }
   virtual void suspend() override;
   void setvars();
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;
   virtual const char* group_label(int id) override;
   virtual int group_label_ypos(int id) override;

private:
   lipol<float, true> feedback;
   static const int n_bq = 4;
   static const int n_bq_units = n_bq << 1;
   float dL, dR;
   BiquadFilter* biquad[8];
   float lfophase;
   int bi; // block increment (to keep track of events not occurring every n blocks)
};

/*	rotary_speaker			*/
class RotarySpeakerEffect : public Effect
{
public:
   lipol_ps width alignas(16), mix alignas(16);

   RotarySpeakerEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~RotarySpeakerEffect();
   virtual void process_only_control() override;
   virtual const char* get_effectname() override
   {
      return "rotary";
   }
   virtual void process(float* dataL, float* dataR) override;
   virtual int get_ringout_decay() override
   {
      return max_delay_length >> 5;
   }
   void setvars(bool init);
   virtual void suspend() override;
   virtual void init() override;
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;
   virtual const char* group_label(int id) override;
   virtual int group_label_ypos(int id) override;

   virtual void handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision) override;

protected:
   float buffer[max_delay_length];
   int wpos;
   // filter *lp[2],*hp[2];
   // biquadunit rotor_lpL,rotor_lpR;
   BiquadFilter xover, lowbass;
   // float
   // f_rotor_lp[2][n_filter_parameters],f_xover[n_filter_parameters],f_lowbass[n_filter_parameters];
   quadr_osc lfo;
   quadr_osc lf_lfo;
   lipol<float> dL, dR, hornamp[2];
   lag<float, true> drive;
   bool first_run;
};

class DistortionEffect : public Effect
{
   HalfRateFilter hr_a alignas(16),
                  hr_b alignas(16);
   lipol_ps drive alignas(16),
            outgain alignas(16);

public:
   DistortionEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~DistortionEffect();
   virtual const char* get_effectname() override
   {
      return "distortion";
   }
   virtual void init() override;
   virtual void process(float* dataL, float* dataR) override;
   virtual void suspend() override;
   virtual int get_ringout_decay() override
   {
      return 1000;
   }
   void setvars(bool init);
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;
   virtual const char* group_label(int id) override;
   virtual int group_label_ypos(int id) override;

   virtual void handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision) override;

private:
   BiquadFilter band1, band2, lp1, lp2;
   int bi; // block increment (to keep track of events not occurring every n blocks)
   float L, R;
};

const int n_vocoder_bands = 20;
const int NVocoderVec = n_vocoder_bands >> 2;

class VocoderEffect : public Effect
{
public:
   enum Params
   {
      KGain,
      KGateLevel,
      KRate,
      KQuality,
      KShift,

      kNumBands,
      kFreqLo,
      kFreqHi,
      kModExpand,
      kModCenter,

      // KUnvoicedThreshold,
   };

   VocoderEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~VocoderEffect();
   virtual const char* get_effectname() override
   {
      return "vocoder";
   }
   virtual void init() override;
   virtual void process(float* dataL, float* dataR) override; 
   virtual void suspend() override;
   virtual int get_ringout_decay() override
   {
      return 500;
   }
   void setvars(bool init);
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;
   virtual const char* group_label(int id) override;
   virtual int group_label_ypos(int id) override;

   virtual void handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision) override;

private:
   VectorizedSvfFilter mCarrierL alignas(16)[NVocoderVec];
   VectorizedSvfFilter mCarrierR alignas(16)[NVocoderVec];
   VectorizedSvfFilter mModulator alignas(16)[NVocoderVec];
   vFloat mEnvF alignas(16)[NVocoderVec];
   lipol_ps mGain alignas(16);

   int mBI; // block increment (to keep track of events not occurring every n blocks)
   int active_bands;
   
   /*
   float mVoicedLevel;
   float mUnvoicedLevel;

   BiquadFilter
           mVoicedDetect,
           mUnvoicedDetect;
   */
};

class emphasize : public Effect
{
   HalfRateFilter pre alignas(16),
                  post alignas(16);
   lipol_ps type alignas(16),
            outgain alignas(16);

public:
   emphasize(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~emphasize();
   virtual const char* get_effectname() override
   {
      return "emphasize";
   }
   virtual void init() override;
   virtual void process(float* dataL, float* dataR) override;
   virtual void suspend() override;
   virtual int get_ringout_decay() override
   {
      return 50;
   }
   void setvars(bool init);
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;
   virtual const char* group_label(int id) override;
   virtual int group_label_ypos(int id) override;

private:
   BiquadFilter EQ;
   int bi; // block increment (to keep track of events not occurring every n blocks)
   float L, R;
};

const int lookahead_bits = 7;
const int lookahead = 1 << 7;

class ConditionerEffect : public Effect
{
   lipol_ps ampL alignas(16),
            ampR alignas(16),
            width alignas(16),
            postamp alignas(16);

public:
   ConditionerEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~ConditionerEffect();
   virtual const char* get_effectname() override
   {
      return "conditioner";
   }
   virtual void init() override;
   virtual void process_only_control() override;
   virtual void process(float* dataL, float* dataR) override;
   virtual int get_ringout_decay() override
   {
      return 100;
   }
   virtual void suspend() override;
   void setvars(bool init);
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;
   virtual int vu_type(int id) override;
   virtual int vu_ypos(int id) override;
   virtual const char* group_label(int id) override;
   virtual int group_label_ypos(int id) override;

private:
   BiquadFilter band1, band2;
   float ef;
   lipol<float, true> a_rate, r_rate;
   float lamax[lookahead << 1];
   float delayed[2][lookahead];
   int bufpos;
   float filtered_lamax, filtered_lamax2, gain;
};

const int revbits = 15;
const int max_rev_dly = 1 << revbits;
const int rev_tap_bits = 4;
const int rev_taps = 1 << rev_tap_bits;

class Reverb1Effect : public Effect
{
   float delay_pan_L alignas(16)[rev_taps],
         delay_pan_R alignas(16)[rev_taps];
   float delay_fb alignas(16)[rev_taps];
   float delay alignas(16)[rev_taps * max_rev_dly];
   float out_tap alignas(16)[rev_taps];
   float predelay alignas(16)[max_rev_dly];
   int delay_time alignas(16)[rev_taps];
   lipol_ps mix alignas(16),
            width alignas(16);

public:
   Reverb1Effect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~Reverb1Effect();
   virtual const char* get_effectname() override
   {
      return "reverb";
   }
   virtual void init() override;
   virtual void process(float* dataL, float* dataR) override;
   virtual void suspend() override;
   void setvars(bool init);
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;
   virtual const char* group_label(int id) override;
   virtual int group_label_ypos(int id) override;
   virtual int get_ringout_decay() override
   {
      return ringout_time;
   }

private:
   void update_rtime();
   void update_rsize();
   void clear_buffers();
   void loadpreset(int id);
   /*int delay_time_mod[rev_taps];
   int delay_time_dv[rev_taps];*/
   int delay_pos;
   /*	float noise[rev_taps];
           float noise_target[rev_taps];*/
   double modphase;
   int shape;
   float lastf[n_fx_params];
   BiquadFilter band1, locut, hicut;
   int ringout_time;
   int b;
};

class Reverb2Effect : public Effect
{
   static const int NUM_BLOCKS = 4,
      NUM_INPUT_ALLPASSES = 4,
      NUM_ALLPASSES_PER_BLOCK = 2,
      MAX_ALLPASS_LEN = 16384,
      MAX_DELAY_LEN = 16384,
      DELAY_LEN_MASK = MAX_DELAY_LEN - 1,
      DELAY_SUBSAMPLE_BITS = 8,
      DELAY_SUBSAMPLE_RANGE = (1 << DELAY_SUBSAMPLE_BITS),
      PREDELAY_BUFFER_SIZE = 48000 * 4 * 4, // max sample rate is 48000 * 4 probably
      PREDELAY_BUFFER_SIZE_LIMIT = 48000 * 4 * 3; // allow for one second of diffusion

   class allpass
   {
   public:
      allpass();
      float process(float x, float coeff);
      void setLen(int len);

   private:
      int _len;
      int _k;
      float _data[MAX_ALLPASS_LEN];
   };

   class delay
   {
   public:
      delay();
      float process(float x, int tap1, float& tap_out1, int tap2, float& tap_out2, int modulation);
      void setLen(int len);

   private:
      int _len;
      int _k;
      float _data[MAX_DELAY_LEN];
   };

   class predelay
   {
   public:
      predelay() {
         memset( _data, 0, PREDELAY_BUFFER_SIZE * sizeof(float));
      }
      float process( float in, int tap ) {
         k = ( k + 1 ); if( k == PREDELAY_BUFFER_SIZE ) k = 0;
         auto p = k - tap; while( p < 0 ) p += PREDELAY_BUFFER_SIZE;
         auto res = _data[p];
         _data[k] = in;
         return res;
      }
   private:
      int k=0;
      float _data[PREDELAY_BUFFER_SIZE];
   };

   class onepole_filter
   {
   public:
      onepole_filter();
      float process_lowpass(float x, float c0);
      float process_highpass(float x, float c0);

   private:
      float a0;
   };

   lipol_ps mix alignas(16),
            width alignas(16);

public:
   Reverb2Effect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~Reverb2Effect();
   virtual const char* get_effectname() override
   {
      return "reverb2";
   }
   virtual void init() override;
   virtual void process(float* dataL, float* dataR) override;
   virtual void suspend() override;
   void setvars(bool init);
   void calc_size(float scale);
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;
   virtual const char* group_label(int id) override;
   virtual int group_label_ypos(int id) override;
   virtual int get_ringout_decay() override
   {
      return ringout_time;
   }


private:
   void update_rtime();
   int ringout_time;
   allpass _input_allpass[NUM_INPUT_ALLPASSES];
   allpass _allpass[NUM_BLOCKS][NUM_ALLPASSES_PER_BLOCK];
   onepole_filter _hf_damper[NUM_BLOCKS];
   onepole_filter _lf_damper[NUM_BLOCKS];
   delay _delay[NUM_BLOCKS];
   predelay _predelay;
   int _tap_timeL[NUM_BLOCKS];
   int _tap_timeR[NUM_BLOCKS];
   float _tap_gainL[NUM_BLOCKS];
   float _tap_gainR[NUM_BLOCKS];
   float _state;
   lipol<float, true> _decay_multiply;
   lipol<float, true> _diffusion;
   lipol<float, true> _buildup;
   lipol<float, true> _hf_damp_coefficent;
   lipol<float, true> _lf_damp_coefficent;
   lipol<float, true> _modulation;
   quadr_osc _lfo;
   float last_decay_time = -1.0;
};

class FlangerEffect : public Effect
{
   enum Modes {
      classic,
      doppler,
      arp_mix,
      arp_solo
   };
   enum Waves {
      sinw,
      triw,
      saww,
      sandhw
   };
   
   static const int COMBS_PER_CHANNEL = 4;
   struct InterpDelay {
      // OK so lets say we want lowest tunable frequency to be 23.5hz at 96k
      // 96000/23.5 = 4084
      // And lets future proof a bit and make it a power of 2 so we can use & properly
      static const int DELAY_SIZE=32768, DELAY_SIZE_MASK = DELAY_SIZE - 1;
      float line[DELAY_SIZE];
      int k = 0;
      InterpDelay() { reset(); }
      void reset() { memset( line, 0, DELAY_SIZE * sizeof( float ) ); k = 0; }
      float value( float delayBy );
      void push( float nv ) {
         k = ( k + 1 ) & DELAY_SIZE_MASK;
         line[k] = nv;
      }
   };
      
public:
   FlangerEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~FlangerEffect();
   virtual const char* get_effectname() override
   {
      return "flanger";
   }
   virtual void init() override;
   virtual void process(float* dataL, float* dataR) override;
   virtual void suspend() override;
   void setvars(bool init);
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;

   virtual const char* group_label(int id) override;
   virtual int group_label_ypos(int id) override;

   virtual int get_ringout_decay() override
   {
      return ringout_value;
   }

private:
   int ringout_value = -1;
   InterpDelay idels[2];

   float lfophase[2][COMBS_PER_CHANNEL], longphase[2];
   float lpaL = 0.f, lpaR = 0.f; // state for the onepole LP filter
   
   lipol<float,true> lfoval[2][COMBS_PER_CHANNEL], delaybase[2][COMBS_PER_CHANNEL];
   lipol<float,true> depth, mix;
   lipol<float,true> voices, voice_detune, voice_chord;
   lipol<float,true> feedback, fb_lf_damping;
   lag<float> vzeropitch;
   float lfosandhtarget[2][COMBS_PER_CHANNEL];
   float vweights[2][COMBS_PER_CHANNEL];

   lipol_ps width;
   bool haveProcessed = false;
   
   const static int LFO_TABLE_SIZE=8192;
   const static int LFO_TABLE_MASK=LFO_TABLE_SIZE-1;
   float sin_lfo_table[LFO_TABLE_SIZE];
   float saw_lfo_table[LFO_TABLE_SIZE]; // don't make it analytic since I want to smooth the edges
};

class RingModulatorEffect : public Effect
{
public:
   static const int MAX_UNISON = 16;
   
   RingModulatorEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~RingModulatorEffect();
   virtual const char* get_effectname() override
   {
      return "ringmodulator";
   }
   virtual void init() override;
   virtual void process(float* dataL, float* dataR) override;
   virtual void suspend() override;
   void setvars(bool init);
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;

   virtual const char* group_label(int id) override;
   virtual int group_label_ypos(int id) override;

   virtual int get_ringout_decay() override
   {
      return ringout_value;
   }

   float diode_sim( float x );

private:
   int ringout_value = -1;
   float phase[MAX_UNISON], detune_offset[MAX_UNISON], panL[MAX_UNISON], panR[MAX_UNISON];
   int last_unison = -1;

   HalfRateFilter halfbandOUT, halfbandIN;
   BiquadFilter lp, hp;
};
